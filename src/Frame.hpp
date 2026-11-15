#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace mjpegproxy {

/// Un frame JPEG con numero progressivo e istante di arrivo. Immutabile una volta pubblicato.
struct Frame {
    std::string jpeg;
    std::uint64_t seq = 0;
    std::chrono::steady_clock::time_point time;
};

using FramePtr = std::shared_ptr<const Frame>;

}  // namespace mjpegproxy
