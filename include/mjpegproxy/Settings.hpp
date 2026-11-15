#pragma once

#include <mjpegproxy/CameraSpec.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mjpegproxy {

/// Configurazione del server.
struct Settings {
    std::vector<CameraSpec> cameras;
    std::string bind         = "0.0.0.0";
    std::uint16_t port       = 8080;
    std::size_t max_clients_per_camera = 32;   // spettatori contemporanei per camera
    std::size_t max_connections        = 256;  // connessioni totali accettate

    /// Interpreta gli argomenti (senza il nome del programma).
    /// Lancia std::invalid_argument con un messaggio leggibile se non sono validi.
    static Settings parse(const std::vector<std::string>& args);

    /// Testo di aiuto per la riga di comando.
    static std::string usage(std::string_view program);
};

}  // namespace mjpegproxy
