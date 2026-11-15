#pragma once

#include <string>
#include <string_view>

namespace mjpegproxy {

/// Indirizzo http di uno stream: host, porta e path.
struct Endpoint {
    std::string host;
    std::string port   = "80";  // stringa, come la vuole il resolver
    std::string target = "/";

    /// "http://host[:porta]/path". Lancia std::invalid_argument se non valido.
    static Endpoint parse(std::string_view url);
};

}  // namespace mjpegproxy
