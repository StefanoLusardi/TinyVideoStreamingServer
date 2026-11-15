#pragma once

#include "Asio.hpp"
#include "FrameSink.hpp"

#include <functional>
#include <string>

namespace mjpegproxy {

/// Origine dei frame di una camera.
///
/// Contratto di stream(): pubblica frame su `sink` finche' keep_going() e' vero,
/// poi ritorna normalmente; su qualunque errore lancia un'eccezione.
/// E' il punto di innesto per le camere finte nei test.
class FrameSource {
public:
    virtual ~FrameSource() = default;

    /// Descrizione per i log (es. "192.168.1.50:81/stream").
    virtual std::string describe() const = 0;

    virtual asio::awaitable<void> stream(FrameSink& sink, std::function<bool()> keep_going) = 0;
};

}  // namespace mjpegproxy
