#include "HttpServer.hpp"

#include "HtmlPages.hpp"
#include "Log.hpp"
#include "Route.hpp"

#include <boost/beast.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace mjpegproxy {

namespace beast = boost::beast;
namespace http  = beast::http;
using namespace std::chrono_literals;

namespace {

// Apre la porta; l'errore di Boost (con percorsi interni) diventa un messaggio leggibile.
tcp::acceptor open_acceptor(const asio::any_io_executor& ex, const tcp::endpoint& endpoint) {
    try {
        return tcp::acceptor(ex, endpoint);
    } catch (const boost::system::system_error& e) {
        throw std::runtime_error("impossibile ascoltare su " + endpoint.address().to_string() + ":" +
                                 std::to_string(endpoint.port()) + ": " + e.code().message());
    }
}

}  // namespace

HttpServer::HttpServer(asio::any_io_executor ex, CameraRegistry& cameras, const tcp::endpoint& listen_on,
                       ServerLimits limits)
    : ex_(std::move(ex)), cameras_(cameras), limits_(limits), acceptor_(open_acceptor(ex_, listen_on)) {
    const auto ep = acceptor_.local_endpoint();
    Log::info("http", "in ascolto su " + ep.address().to_string() + ":" + std::to_string(ep.port()));
}

asio::awaitable<void> HttpServer::accept_loop() {
    for (;;) {
        auto [ec, sock] = co_await acceptor_.async_accept(kNoErr);
        if (ec) {  // es. EMFILE: evito il busy loop
            asio::steady_timer t(ex_, 100ms);
            co_await t.async_wait(kNoErr);
            continue;
        }
        if (*active_ >= limits_.max_connections) continue;  // troppe connessioni: il socket si chiude

        sock.set_option(tcp::no_delay(true));
        ++*active_;  // contato qui, prima dell'avvio della coroutine
        asio::co_spawn(ex_, serve_counted(std::move(sock), active_), asio::detached);
    }
}

asio::awaitable<void> HttpServer::serve_counted(tcp::socket sock, std::shared_ptr<std::size_t> counter) {
    struct Release {
        std::shared_ptr<std::size_t> counter;
        ~Release() { --*counter; }
    } release{counter};
    co_await serve(std::move(sock));
}

asio::awaitable<void> HttpServer::serve(tcp::socket sock) {
    beast::tcp_stream stream(std::move(sock));
    beast::flat_buffer buf;
    http::request<http::empty_body> req;
    stream.expires_after(limits_.request_timeout);
    if (auto [ec, n] = co_await http::async_read(stream, buf, req, kNoErr); ec) co_return;

    if (req.method() != http::verb::get) co_return co_await send_simple(stream, "405 Method Not Allowed", "text/plain", "");

    const Route route = Route::parse({req.target().data(), req.target().size()});

    if (route.kind == Route::Kind::Health) co_return co_await send_simple(stream, "200 OK", "text/plain", "ok\n");

    std::shared_ptr<Camera> camera;
    if (route.kind != Route::Kind::Index && route.kind != Route::Kind::NotFound) camera = cameras_.find(route.camera);

    // Pagine HTML (indice o singola camera): body calcolato prima dei co_await.
    std::string page;
    if (route.kind == Route::Kind::Index) page = HtmlPages::index(cameras_.names());
    else if (camera && route.kind == Route::Kind::CameraPage) page = HtmlPages::index(std::vector<std::string>{camera->name()});
    if (!page.empty()) co_return co_await send_simple(stream, "200 OK", "text/html; charset=utf-8", page);

    if (!camera) co_return co_await send_simple(stream, "404 Not Found", "text/plain", "not found\n");
    if (camera->hub().subscriber_count() >= limits_.max_clients_per_camera)
        co_return co_await send_simple(stream, "503 Service Unavailable", "text/plain", "troppi client\n");
    if (route.kind == Route::Kind::Stream) co_return co_await stream_to(stream, camera);
    co_return co_await snapshot_to(stream, camera);
}

asio::awaitable<void> HttpServer::send_simple(beast::tcp_stream& s, std::string_view status, std::string_view type,
                                              std::string_view body) {
    const std::string response = "HTTP/1.1 " + std::string(status) + "\r\nContent-Type: " + std::string(type) +
                                 "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" +
                                 std::string(body);
    s.expires_after(limits_.write_timeout);
    co_await asio::async_write(s, asio::buffer(response), kNoErr);
}

// `camera` per valore: tiene viva la camera finche' dura questa coroutine.
asio::awaitable<void> HttpServer::stream_to(beast::tcp_stream& s, std::shared_ptr<Camera> camera) {
    FrameHub::Subscriber sub(camera->hub());  // da qui la camera si connette

    // Frame recente: lo mando subito. Frame vecchio (camera ferma): aspetto il prossimo.
    std::uint64_t last = 0;
    if (auto f = camera->hub().latest()) last = (Clock::now() - f->time < 2s) ? f->seq - 1 : f->seq;

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

asio::awaitable<void> HttpServer::snapshot_to(beast::tcp_stream& s, std::shared_ptr<Camera> camera) {
    FrameHub::Subscriber sub(camera->hub());
    auto f = camera->hub().latest();
    if (!f || Clock::now() - f->time > 1s) f = co_await sub.next(f ? f->seq : 0, 5s);
    if (!f) co_return co_await send_simple(s, "503 Service Unavailable", "text/plain", "nessun frame dalla camera\n");

    const std::string head = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nCache-Control: no-store\r\n"
                             "Content-Length: " + std::to_string(f->jpeg.size()) + "\r\nConnection: close\r\n\r\n";
    const std::array<asio::const_buffer, 2> bufs{asio::buffer(head), asio::buffer(f->jpeg)};
    s.expires_after(limits_.write_timeout);
    co_await asio::async_write(s, bufs, kNoErr);
}

}  // namespace mjpegproxy
