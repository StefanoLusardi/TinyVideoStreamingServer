#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace mjpegproxy {

inline constexpr std::size_t kMaxFrameBuffer = 4 * 1024 * 1024;  // limite di sicurezza per un frame

/// Riceve i byte del body di un multipart/x-mixed-replace (gia' de-chunkati) e
/// produce un JPEG per ogni parte. Usa il Content-Length della parte se c'e'
/// (frame emesso subito), altrimenti il delimitatore successivo.
/// Logica pura: nessun I/O.
class MultipartSplitter {
public:
    using FrameCallback = std::function<void(std::string)>;

    explicit MultipartSplitter(std::string boundary);

    /// "multipart/x-mixed-replace;boundary=XYZ" -> "XYZ". Lancia std::runtime_error se manca.
    static std::string boundary_from(std::string_view content_type);

    /// Aggiunge dati e chiama on_frame per ogni JPEG completo.
    void feed(std::string_view data, const FrameCallback& on_frame);

private:
    void guard();

    std::string delim_;
    std::string buf_;
};

}  // namespace mjpegproxy
