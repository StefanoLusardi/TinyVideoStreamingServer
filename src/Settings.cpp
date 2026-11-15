#include <mjpegproxy/Settings.hpp>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace mjpegproxy {

namespace {

std::size_t parse_count(const std::string& option, const std::string& value, std::size_t min, std::size_t max) {
    std::size_t v        = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
    if (ec != std::errc{} || end != value.data() + value.size() || v < min || v > max)
        throw std::invalid_argument("valore non valido per " + option + ": '" + value + "'");
    return v;
}

}  // namespace

Settings Settings::parse(const std::vector<std::string>& args) {
    Settings s;
    auto value_of = [&](std::size_t& i) -> const std::string& {
        if (i + 1 >= args.size()) throw std::invalid_argument("manca il valore dopo " + args[i]);
        return args[++i];
    };

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--port") {
            s.port = static_cast<std::uint16_t>(parse_count(a, value_of(i), 1, 65535));
        } else if (a == "--bind") {
            const std::string& v = value_of(i);
            boost::system::error_code ec;
            boost::asio::ip::make_address(v, ec);
            if (ec) throw std::invalid_argument("indirizzo di bind non valido: '" + v + "'");
            s.bind = v;
        } else if (a == "--max-clients") {
            s.max_clients_per_camera = parse_count(a, value_of(i), 1, 100000);
        } else if (a == "--max-connections") {
            s.max_connections = parse_count(a, value_of(i), 1, 100000);
        } else if (a.starts_with("--")) {
            throw std::invalid_argument("opzione sconosciuta: " + a);
        } else {
            auto spec = CameraSpec::parse(a);
            if (std::ranges::any_of(s.cameras, [&](const CameraSpec& c) { return c.name == spec.name; }))
                throw std::invalid_argument("nome camera duplicato: " + spec.name);
            s.cameras.push_back(std::move(spec));
        }
    }
    if (s.cameras.empty()) throw std::invalid_argument("serve almeno una camera");
    return s;
}

std::string Settings::usage(std::string_view program) {
    return "uso: " + std::string(program) +
           " [opzioni] <nome>=<http://host[:porta]/path> [...]\n"
           "\n"
           "opzioni:\n"
           "  --port N              porta HTTP (default 8080)\n"
           "  --bind ADDR           indirizzo di ascolto (default 0.0.0.0)\n"
           "  --max-clients N       spettatori massimi per camera (default 32)\n"
           "  --max-connections N   connessioni totali massime (default 256)\n"
           "  -h, --help            questo messaggio\n"
           "\n"
           "esempio:\n"
           "  " + std::string(program) + " --port 8080 salotto=http://192.168.1.50:81/stream\n";
}

}  // namespace mjpegproxy
