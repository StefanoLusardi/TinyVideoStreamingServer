#include <mjpegproxy/Endpoint.hpp>

#include <charconv>
#include <stdexcept>

namespace mjpegproxy {

Endpoint Endpoint::parse(std::string_view url) {
    constexpr std::string_view scheme = "http://";
    if (!url.starts_with(scheme)) throw std::invalid_argument("supportato solo http://host[:porta]/path");
    url.remove_prefix(scheme.size());

    const auto slash    = url.find('/');
    const auto hostport = url.substr(0, slash);

    Endpoint ep;
    if (slash != std::string_view::npos) ep.target = std::string(url.substr(slash));

    if (const auto colon = hostport.rfind(':'); colon != std::string_view::npos) {
        ep.host = hostport.substr(0, colon);
        const auto port = hostport.substr(colon + 1);
        int value       = 0;
        const auto [end, ec] = std::from_chars(port.data(), port.data() + port.size(), value);
        if (ec != std::errc{} || end != port.data() + port.size() || value < 1 || value > 65535)
            throw std::invalid_argument("porta non valida: '" + std::string(port) + "'");
        ep.port = std::string(port);
    } else {
        ep.host = hostport;
    }
    if (ep.host.empty()) throw std::invalid_argument("host mancante");
    return ep;
}

}  // namespace mjpegproxy
