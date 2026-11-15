#pragma once

#include <string>
#include <string_view>

namespace mjpegproxy {

/// Cosa chiede un client, dalla sola sintassi dell'URL (non sa quali camere esistano).
struct Route {
    enum class Kind { Index, Health, CameraPage, Stream, Snapshot, NotFound };

    Kind kind = Kind::NotFound;
    std::string camera;  // valorizzato per CameraPage, Stream, Snapshot

    /// "/", "/healthz", "/<nome>/", "/<nome>/stream[.mjpeg]", "/<nome>/snapshot.jpg"; ignora la query.
    static Route parse(std::string_view target);
};

}  // namespace mjpegproxy
