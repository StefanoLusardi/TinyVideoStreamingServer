#pragma once

#include <mjpegproxy/Endpoint.hpp>

#include <string>
#include <string_view>

namespace mjpegproxy {

/// Una camera configurata: nome pubblico (usato nelle URL) e indirizzo dello stream.
struct CameraSpec {
    std::string name;
    Endpoint endpoint;

    /// Lettere, cifre, '-' e '_'; da 1 a 32 caratteri.
    static bool is_valid_name(std::string_view name);

    /// "nome=http://host[:porta]/path" oppure un URL nudo (nome "cam").
    /// Lancia std::invalid_argument se non valido.
    static CameraSpec parse(std::string_view arg);
};

}  // namespace mjpegproxy
