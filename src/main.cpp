// mjpeg-proxy: legge lo stream MJPEG di una o piu' camere ESP32 (firmware
// standard CameraWebServer) con UNA sola connessione per camera, e lo
// ri-trasmette a N client HTTP.
//
// Uso:
//   mjpeg-proxy [--port N] [--bind ADDR] <nome>=<http://host[:porta]/path> [...]
//   mjpeg-proxy --port 8080 salotto=http://192.168.1.50:81/stream garage=http://192.168.1.51:81/stream
//
// Endpoint:
//   /                     pagina con tutte le camere
//   /<nome>/              pagina della singola camera
//   /<nome>/stream        stream MJPEG (multipart/x-mixed-replace)
//   /<nome>/snapshot.jpg  ultimo frame JPEG
//
// Struttura (tutto in questo file, dall'alto verso il basso; ogni livello usa solo i precedenti):
//   1. Config      parse_endpoint / parse_camera_spec / parse_settings    (funzioni pure)
//   2. Multipart   MultipartSplitter                                      (logica pura, nessun I/O)
//   3. Frames      FrameSink (interfaccia), FrameHub                      (ultimo frame + fan-out)
//   4. Sources     FrameSource (interfaccia), HttpMjpegSource             (lato ESP32)
//   5. Camera      Camera, CameraRegistry                                 (ciclo di vita per camera)
//   6. Http        parse_route, render_index, HttpServer                  (lato client)
//   7. main        (escludibile con -DMJPEG_PROXY_NO_MAIN per includere il file nei test)
//
// Punti di innesto per i test: FrameSource (camera finta), FrameSink/FrameHub,
// MultipartSplitter, le funzioni di parsing e HttpServer::serve() su un socket locale.
//
// Un solo thread, un solo io_context, coroutine C++20: nessun lock.

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using asio::use_awaitable;
using asio::ip::tcp;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr auto kNoErr = asio::as_tuple(use_awaitable);        // co_await che restituisce {ec, ...} senza eccezioni
constexpr std::size_t kMaxFrameBuffer = 4 * 1024 * 1024;      // limite di sicurezza per un frame

inline void log(std::string_view who, std::string_view msg) {
    std::fprintf(stderr, "[%.*s] %.*s\n", static_cast<int>(who.size()), who.data(), static_cast<int>(msg.size()), msg.data());
}

inline std::string to_lower(std::string_view s) {
    std::string r(s);
    std::ranges::transform(r, r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

// =============================================================== 1. Config

struct Endpoint {
    std::string host, port, target;
};

struct CameraSpec {
    std::string name;
    Endpoint endpoint;
};

struct Settings {
    std::vector<CameraSpec> cameras;
    asio::ip::address bind = asio::ip::address_v4::any();
    std::uint16_t port     = 8080;
};

inline bool is_valid_camera_name(std::string_view n) {
    return !n.empty() && n.size() <= 32 &&
           std::ranges::all_of(n, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

// "http://host[:porta]/path" -> Endpoint (porta di default 80, path di default "/")
inline Endpoint parse_endpoint(std::string_view url) {
    constexpr std::string_view scheme = "http://";
    if (!url.starts_with(scheme)) throw std::invalid_argument("supportato solo http://host[:porta]/path");
    url.remove_prefix(scheme.size());
    const auto slash    = url.find('/');
    const auto hostport = url.substr(0, slash);
    Endpoint ep;
    ep.target = slash == std::string_view::npos ? "/" : std::string(url.substr(slash));
    if (const auto colon = hostport.rfind(':'); colon != std::string_view::npos) {
        ep.host = hostport.substr(0, colon);
        ep.port = hostport.substr(colon + 1);
    } else {
        ep.host = hostport;
        ep.port = "80";
    }
    if (ep.host.empty()) throw std::invalid_argument("host mancante");
    return ep;
}

// "nome=http://..." oppure, per comodita', un URL nudo (nome "cam").
inline CameraSpec parse_camera_spec(std::string_view arg) {
    std::string name = "cam";
    if (!arg.starts_with("http://")) {
        const auto eq = arg.find('=');
        if (eq == std::string_view::npos)
            throw std::invalid_argument("camera '" + std::string(arg) + "': atteso nome=http://host[:porta]/path");
        name = std::string(arg.substr(0, eq));
        arg.remove_prefix(eq + 1);
    }
    if (!is_valid_camera_name(name))
        throw std::invalid_argument("nome camera non valido '" + name + "' (ammessi: lettere, cifre, - _ ; max 32)");
    return {std::move(name), parse_endpoint(arg)};
}

inline Settings parse_settings(const std::vector<std::string>& args) {
    Settings s;
    auto value_of = [&](std::size_t& i) -> const std::string& {
        if (i + 1 >= args.size()) throw std::invalid_argument("manca il valore dopo " + args[i]);
        return args[++i];
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--port") {
            const auto& v = value_of(i);
            int port      = 0;
            auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), port);
            if (ec != std::errc{} || end != v.data() + v.size() || port < 1 || port > 65535)
                throw std::invalid_argument("porta non valida: " + v);
            s.port = static_cast<std::uint16_t>(port);
        } else if (a == "--bind") {
            boost::system::error_code ec;
            s.bind = asio::ip::make_address(value_of(i), ec);
            if (ec) throw std::invalid_argument("indirizzo di bind non valido: " + args[i]);
        } else if (a.starts_with("--")) {
            throw std::invalid_argument("opzione sconosciuta: " + a);
        } else {
            auto spec = parse_camera_spec(a);
            if (std::ranges::any_of(s.cameras, [&](const CameraSpec& c) { return c.name == spec.name; }))
                throw std::invalid_argument("nome camera duplicato: " + spec.name);
            s.cameras.push_back(std::move(spec));
        }
    }
    if (s.cameras.empty()) throw std::invalid_argument("serve almeno una camera");
    return s;
}

// ============================================================ 2. Multipart

// Riceve i byte del body (gia' de-chunkati) e produce un JPEG per ogni parte.
// Usa Content-Length della parte se presente (frame emesso subito), altrimenti
// il delimitatore successivo.
class MultipartSplitter {
public:
    explicit MultipartSplitter(std::string boundary) : delim_("--" + std::move(boundary)) {}

    // "multipart/x-mixed-replace;boundary=XYZ" -> "XYZ"
    static std::string boundary_from(std::string_view content_type) {
        const auto lower = to_lower(content_type);
        const auto p     = lower.find("boundary=");
        if (p == std::string::npos) throw std::runtime_error("la camera non risponde con un multipart");
        auto v = content_type.substr(p + 9);
        if (const auto e = v.find(';'); e != std::string_view::npos) v = v.substr(0, e);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        return std::string(v);
    }

    template <class OnFrame>
    void feed(std::string_view data, OnFrame&& on_frame) {
        buf_.append(data);
        for (;;) {
            const auto d = buf_.find(delim_);
            if (d == std::string::npos) return guard();
            if (d > 0) buf_.erase(0, d);  // scarta quel che precede il delimitatore

            const auto h = buf_.find("\r\n\r\n");
            if (h == std::string::npos) return guard();
            const std::size_t body = h + 4;
            const std::string_view headers(buf_.data() + delim_.size(), h - delim_.size());

            if (const auto len = content_length(headers)) {
                if (buf_.size() < body + *len) return guard();
                emit(buf_.substr(body, *len), on_frame);
                buf_.erase(0, body + *len);
            } else {
                const auto n = buf_.find(delim_, body);
                if (n == std::string::npos) return guard();
                std::size_t end = n;
                if (end >= body + 2 && buf_.compare(end - 2, 2, "\r\n") == 0) end -= 2;
                emit(buf_.substr(body, end - body), on_frame);
                buf_.erase(0, n);
            }
        }
    }

private:
    static std::optional<std::size_t> content_length(std::string_view headers) {
        const auto lower = to_lower(headers);
        auto p           = lower.find("content-length:");
        if (p == std::string::npos) return std::nullopt;
        p += 15;
        while (p < lower.size() && lower[p] == ' ') ++p;
        std::size_t v  = 0;
        auto [end, ec] = std::from_chars(lower.data() + p, lower.data() + lower.size(), v);
        if (ec != std::errc{} || end == lower.data() + p || v > kMaxFrameBuffer) return std::nullopt;
        return v;
    }

    void guard() {
        if (buf_.size() > kMaxFrameBuffer) buf_.clear();  // stream corrotto: risincronizza
    }

    template <class OnFrame>
    static void emit(std::string jpeg, OnFrame& on_frame) {
        // SOI JPEG: FF D8
        if (jpeg.size() > 4 && static_cast<unsigned char>(jpeg[0]) == 0xFF && static_cast<unsigned char>(jpeg[1]) == 0xD8)
            on_frame(std::move(jpeg));
    }

    std::string delim_;
    std::string buf_;
};

// ============================================================== 3. Frames

struct Frame {
    std::string jpeg;
    std::uint64_t seq;
    Clock::time_point time;
};
using FramePtr = std::shared_ptr<const Frame>;

// Dove una FrameSource deposita i frame.
class FrameSink {
public:
    virtual ~FrameSink()                  = default;
    virtual void publish(std::string jpeg) = 0;
};

// Ultimo frame di una camera + fan-out verso i client (Subscriber).
class FrameHub final : public FrameSink {
public:
    explicit FrameHub(asio::any_io_executor ex) : ex_(std::move(ex)), idle_(ex_, Clock::time_point::max()) {}

    // Un client interessato ai frame. Finche' esiste, la camera resta connessa.
    class Subscriber {
    public:
        explicit Subscriber(FrameHub& hub) : hub_(hub), wake_(hub.ex_, Clock::time_point::max()) {
            hub_.subs_.insert(this);
            hub_.idle_.cancel();  // sveglia chi attende il primo spettatore
        }
        ~Subscriber() { hub_.subs_.erase(this); }
        Subscriber(const Subscriber&)            = delete;
        Subscriber& operator=(const Subscriber&) = delete;

        void notify() { wake_.cancel(); }

        // Attende un frame con seq > last_seq. nullptr se scade il timeout.
        asio::awaitable<FramePtr> next(std::uint64_t last_seq, Clock::duration timeout) {
            const auto deadline = Clock::now() + timeout;
            for (;;) {
                if (auto f = hub_.latest_; f && f->seq > last_seq) co_return f;
                if (Clock::now() >= deadline) co_return nullptr;
                wake_.expires_at(deadline);
                co_await wake_.async_wait(kNoErr);  // cancel() = nuovo frame; scadenza = timeout
            }
        }

    private:
        FrameHub& hub_;
        asio::steady_timer wake_;
    };

    void publish(std::string jpeg) override {
        latest_ = std::make_shared<const Frame>(Frame{std::move(jpeg), ++seq_, Clock::now()});
        for (auto* s : subs_) s->notify();
    }

    FramePtr latest() const { return latest_; }
    std::size_t subscriber_count() const { return subs_.size(); }

    asio::awaitable<void> wait_for_subscribers() {
        while (subs_.empty()) {
            idle_.expires_at(Clock::time_point::max());
            co_await idle_.async_wait(kNoErr);
        }
    }

private:
    asio::any_io_executor ex_;
    asio::steady_timer idle_;
    std::set<Subscriber*> subs_;
    FramePtr latest_;
    std::uint64_t seq_ = 0;
};

// ============================================================== 4. Sources

// Origine dei frame di una camera. Contratto di stream(): pubblica frame su
// `sink` finche' keep_going() e' vero, poi ritorna; su qualunque errore lancia.
class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual std::string describe() const = 0;
    virtual asio::awaitable<void> stream(FrameSink& sink, std::function<bool()> keep_going) = 0;
};

// ESP32 con firmware standard: GET http://host:81/stream (multipart, chunked).
class HttpMjpegSource final : public FrameSource {
public:
    explicit HttpMjpegSource(Endpoint ep, std::chrono::seconds io_timeout = 5s) : ep_(std::move(ep)), timeout_(io_timeout) {}

    std::string describe() const override { return ep_.host + ":" + ep_.port + ep_.target; }

    asio::awaitable<void> stream(FrameSink& sink, std::function<bool()> keep_going) override {
        auto ex = co_await asio::this_coro::executor;

        tcp::resolver resolver(ex);
        const auto endpoints = co_await resolver.async_resolve(ep_.host, ep_.port, use_awaitable);

        beast::tcp_stream conn(ex);
        conn.expires_after(timeout_);
        co_await conn.async_connect(endpoints, use_awaitable);

        http::request<http::empty_body> req{http::verb::get, ep_.target, 11};
        req.set(http::field::host, ep_.host);
        req.set(http::field::user_agent, "mjpeg-proxy");
        conn.expires_after(timeout_);
        co_await http::async_write(conn, req, use_awaitable);

        beast::flat_buffer buf;
        http::response_parser<http::buffer_body> parser;
        parser.body_limit(boost::none);  // stream infinito
        conn.expires_after(timeout_);
        co_await http::async_read_header(conn, buf, parser, use_awaitable);

        if (parser.get().result() != http::status::ok)
            throw std::runtime_error("la camera ha risposto " + std::to_string(parser.get().result_int()));

        const auto ct = parser.get()[http::field::content_type];
        MultipartSplitter splitter(MultipartSplitter::boundary_from({ct.data(), ct.size()}));

        std::array<char, 16 * 1024> chunk;
        while (keep_going()) {
            auto& body = parser.get().body();
            body.data  = chunk.data();
            body.size  = chunk.size();

            conn.expires_after(timeout_);  // niente dati per timeout_ => errore => riconnessione
            auto [ec, n] = co_await http::async_read_some(conn, buf, parser, kNoErr);
            if (ec && ec != http::error::need_buffer) throw beast::system_error(ec);

            splitter.feed({chunk.data(), chunk.size() - body.size}, [&](std::string jpeg) { sink.publish(std::move(jpeg)); });
            if (parser.is_done()) throw std::runtime_error("la camera ha chiuso lo stream");
        }
    }

private:
    Endpoint ep_;
    std::chrono::seconds timeout_;
};

// ============================================================== 5. Camera

struct CameraOptions {
    std::chrono::milliseconds retry_delay = 1s;  // attesa dopo un errore dell'origine
};

// Una camera: il suo hub di frame e la sua origine. La sessione verso l'origine
// esiste solo finche' c'e' almeno uno spettatore; dopo un errore si riprova.
class Camera {
public:
    Camera(std::string name, asio::any_io_executor ex, std::unique_ptr<FrameSource> source, CameraOptions opt = {})
        : name_(std::move(name)), ex_(ex), hub_(ex), source_(std::move(source)), opt_(opt) {}

    const std::string& name() const { return name_; }
    FrameHub& hub() { return hub_; }

    // Supervisore (da lanciare con co_spawn). Prende lo shared_ptr per tenere
    // viva la camera finche' la coroutine esiste.
    static asio::awaitable<void> supervise(std::shared_ptr<Camera> self) {
        for (;;) {
            co_await self->hub_.wait_for_subscribers();
            bool failed = false;
            try {
                log(self->name_, "connessione a " + self->source_->describe());
                FrameHub& hub = self->hub_;
                co_await self->source_->stream(hub, [&hub] { return hub.subscriber_count() > 0; });
                log(self->name_, "nessuno spettatore: connessione chiusa");
            } catch (const boost::system::system_error& e) {  // niente co_await dentro catch
                log(self->name_, e.code().message() + " (riprovo)");
                failed = true;
            } catch (const std::exception& e) {
                log(self->name_, std::string(e.what()) + " (riprovo)");
                failed = true;
            }
            if (failed) {
                asio::steady_timer backoff(self->ex_, self->opt_.retry_delay);
                co_await backoff.async_wait(kNoErr);
            }
        }
    }

private:
    std::string name_;
    asio::any_io_executor ex_;
    FrameHub hub_;
    std::unique_ptr<FrameSource> source_;
    CameraOptions opt_;
};

class CameraRegistry {
public:
    void add(std::shared_ptr<Camera> cam) {
        const std::string name = cam->name();
        if (!by_name_.emplace(name, std::move(cam)).second) throw std::invalid_argument("camera duplicata: " + name);
        order_.push_back(name);
    }
    std::shared_ptr<Camera> find(std::string_view name) const {
        const auto it = by_name_.find(name);
        return it == by_name_.end() ? nullptr : it->second;
    }
    const std::vector<std::string>& names() const { return order_; }

    void start_all(const asio::any_io_executor& ex) const {
        for (const auto& [name, cam] : by_name_) asio::co_spawn(ex, Camera::supervise(cam), asio::detached);
    }

private:
    std::map<std::string, std::shared_ptr<Camera>, std::less<>> by_name_;
    std::vector<std::string> order_;  // ordine di configurazione, per la pagina indice
};

inline CameraRegistry build_registry(const Settings& settings, const asio::any_io_executor& ex) {
    CameraRegistry reg;
    for (const auto& spec : settings.cameras)
        reg.add(std::make_shared<Camera>(spec.name, ex, std::make_unique<HttpMjpegSource>(spec.endpoint)));
    return reg;
}

// ================================================================= 6. Http

struct Route {
    enum class Kind { Index, CameraPage, Stream, Snapshot, NotFound };
    Kind kind = Kind::NotFound;
    std::string camera;
};

// Solo sintassi: non sa quali camere esistano.
inline Route parse_route(std::string_view target) {
    target = target.substr(0, target.find('?'));
    if (target == "/") return {Route::Kind::Index, {}};
    if (!target.starts_with('/')) return {};
    target.remove_prefix(1);
    const auto slash = target.find('/');
    const auto name  = target.substr(0, slash);
    if (!is_valid_camera_name(name)) return {};

    Route r{Route::Kind::NotFound, std::string(name)};
    const auto rest = slash == std::string_view::npos ? std::string_view{} : target.substr(slash + 1);
    if (rest.empty()) r.kind = Route::Kind::CameraPage;
    else if (rest == "stream" || rest == "stream.mjpeg") r.kind = Route::Kind::Stream;
    else if (rest == "snapshot.jpg") r.kind = Route::Kind::Snapshot;
    return r;
}

// I nomi sono gia' validati ([A-Za-z0-9_-]): nessun escaping necessario.
inline std::string render_index(const std::vector<std::string>& names) {
    std::string h =
        "<!doctype html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>Camere</title><style>body{margin:0;background:#111;color:#ccc;font:14px system-ui}"
        "main{display:grid;gap:8px;padding:8px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}"
        "figure{margin:0}img{width:100%;display:block;background:#000}a{color:inherit}figcaption{padding:4px 0}"
        "</style><main>";
    for (const auto& n : names)
        h += "<figure><a href=\"/" + n + "/\"><img src=\"/" + n + "/stream\" alt=\"" + n + "\"></a><figcaption>" + n +
             "</figcaption></figure>";
    return h + "</main>";
}

struct ServerLimits {
    std::size_t max_clients_per_camera = 32;
    std::chrono::seconds request_timeout{10};
    std::chrono::seconds write_timeout{5};  // client lento o morto => lo scarto
};

class HttpServer {
public:
    HttpServer(asio::any_io_executor ex, CameraRegistry& cameras, ServerLimits limits = {})
        : ex_(std::move(ex)), cameras_(cameras), limits_(limits) {}

    asio::awaitable<void> listen(tcp::endpoint ep) {
        tcp::acceptor acceptor(ex_, ep);
        log("http", "in ascolto su " + ep.address().to_string() + ":" + std::to_string(ep.port()));
        for (;;) {
            auto [ec, sock] = co_await acceptor.async_accept(kNoErr);
            if (ec) {  // es. EMFILE: evito il busy loop
                asio::steady_timer t(ex_, 100ms);
                co_await t.async_wait(kNoErr);
                continue;
            }
            sock.set_option(tcp::no_delay(true));
            asio::co_spawn(ex_, serve(std::move(sock)), asio::detached);
        }
    }

    // Una connessione client (pubblica: i test possono passare un socket gia' connesso).
    asio::awaitable<void> serve(tcp::socket sock) {
        beast::tcp_stream stream(std::move(sock));
        beast::flat_buffer buf;
        http::request<http::empty_body> req;
        stream.expires_after(limits_.request_timeout);
        if (auto [ec, n] = co_await http::async_read(stream, buf, req, kNoErr); ec) co_return;

        if (req.method() != http::verb::get)
            co_return co_await send_simple(stream, "405 Method Not Allowed", "text/plain", "");

        const Route route = parse_route({req.target().data(), req.target().size()});

        std::shared_ptr<Camera> cam;
        if (route.kind != Route::Kind::Index && route.kind != Route::Kind::NotFound) cam = cameras_.find(route.camera);

        // Pagine HTML (indice o singola camera): body calcolato prima dei co_await.
        std::string page;
        if (route.kind == Route::Kind::Index) page = render_index(cameras_.names());
        else if (cam && route.kind == Route::Kind::CameraPage) page = render_index(std::vector<std::string>{cam->name()});
        if (!page.empty()) co_return co_await send_simple(stream, "200 OK", "text/html; charset=utf-8", page);

        if (!cam) co_return co_await send_simple(stream, "404 Not Found", "text/plain", "not found\n");
        if (cam->hub().subscriber_count() >= limits_.max_clients_per_camera)
            co_return co_await send_simple(stream, "503 Service Unavailable", "text/plain", "troppi client\n");
        if (route.kind == Route::Kind::Stream) co_return co_await stream_to(stream, cam);
        co_return co_await snapshot_to(stream, cam);
    }

private:
    asio::awaitable<void> send_simple(beast::tcp_stream& s, std::string_view status, std::string_view type,
                                      std::string_view body) {
        const std::string r = "HTTP/1.1 " + std::string(status) + "\r\nContent-Type: " + std::string(type) +
                              "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" +
                              std::string(body);
        s.expires_after(limits_.write_timeout);
        co_await asio::async_write(s, asio::buffer(r), kNoErr);
    }

    // `cam` per valore: tiene viva la camera finche' dura questa coroutine.
    asio::awaitable<void> stream_to(beast::tcp_stream& s, std::shared_ptr<Camera> cam) {
        FrameHub::Subscriber sub(cam->hub());  // da qui la camera si connette

        // Frame recente: lo mando subito. Frame vecchio (camera ferma): aspetto il prossimo.
        std::uint64_t last = 0;
        if (auto f = cam->hub().latest()) last = (Clock::now() - f->time < 2s) ? f->seq - 1 : f->seq;

        static constexpr std::string_view head =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=mjpegproxyframe\r\n"
            "Cache-Control: no-store\r\nPragma: no-cache\r\nConnection: close\r\n\r\n";
        s.expires_after(limits_.write_timeout);
        if (auto [ec, n] = co_await asio::async_write(s, asio::buffer(head), kNoErr); ec) co_return;

        for (;;) {
            auto f = co_await sub.next(last, 10s);
            if (!f) continue;  // camera ferma: resto in attesa
            last = f->seq;

            const std::string part = "--mjpegproxyframe\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                     std::to_string(f->jpeg.size()) + "\r\n\r\n";
            const std::array<asio::const_buffer, 3> bufs{asio::buffer(part), asio::buffer(f->jpeg), asio::buffer("\r\n", 2)};
            s.expires_after(limits_.write_timeout);
            if (auto [ec, n] = co_await asio::async_write(s, bufs, kNoErr); ec) co_return;
        }
    }

    asio::awaitable<void> snapshot_to(beast::tcp_stream& s, std::shared_ptr<Camera> cam) {
        FrameHub::Subscriber sub(cam->hub());
        auto f = cam->hub().latest();
        if (!f || Clock::now() - f->time > 1s) f = co_await sub.next(f ? f->seq : 0, 5s);
        if (!f) co_return co_await send_simple(s, "503 Service Unavailable", "text/plain", "nessun frame dalla camera\n");

        const std::string head = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nCache-Control: no-store\r\n"
                                 "Content-Length: " + std::to_string(f->jpeg.size()) + "\r\nConnection: close\r\n\r\n";
        const std::array<asio::const_buffer, 2> bufs{asio::buffer(head), asio::buffer(f->jpeg)};
        s.expires_after(limits_.write_timeout);
        co_await asio::async_write(s, bufs, kNoErr);
    }

    asio::any_io_executor ex_;
    CameraRegistry& cameras_;
    ServerLimits limits_;
};

// ================================================================== 7. main

#ifndef MJPEG_PROXY_NO_MAIN
int main(int argc, char** argv) {
    Settings settings;
    try {
        settings = parse_settings(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const std::invalid_argument& e) {
        std::fprintf(stderr, "errore: %s\n\nuso: %s [--port N] [--bind ADDR] <nome>=<http://host[:porta]/path> [...]\n", e.what(),
                     argv[0]);
        return 2;
    }
    try {
        asio::io_context io{1};
        auto registry = build_registry(settings, io.get_executor());
        HttpServer server(io.get_executor(), registry);

        registry.start_all(io.get_executor());
        asio::co_spawn(io, server.listen({settings.bind, settings.port}), asio::detached);

        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io.stop(); });
        io.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "errore: %s\n", e.what());
        return 1;
    }
}
#endif
