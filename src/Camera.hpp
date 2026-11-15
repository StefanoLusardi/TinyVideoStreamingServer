#pragma once

#include "Asio.hpp"
#include "FrameHub.hpp"
#include "FrameSource.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace mjpegproxy {

struct CameraOptions {
    std::chrono::milliseconds retry_delay{1000};  // attesa dopo un errore dell'origine
};

/// Una camera: il suo hub di frame e la sua origine. La sessione verso l'origine
/// esiste solo finche' c'e' almeno uno spettatore; dopo un errore si riprova.
class Camera {
public:
    Camera(std::string name, asio::any_io_executor ex, std::unique_ptr<FrameSource> source, CameraOptions options = {});

    const std::string& name() const;
    FrameHub& hub();

    /// Supervisore da lanciare con co_spawn. Prende lo shared_ptr per tenere viva
    /// la camera finche' la coroutine esiste.
    static asio::awaitable<void> supervise(std::shared_ptr<Camera> self);

private:
    std::string name_;
    asio::any_io_executor ex_;
    FrameHub hub_;
    std::unique_ptr<FrameSource> source_;
    CameraOptions options_;
};

}  // namespace mjpegproxy
