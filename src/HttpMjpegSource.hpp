#pragma once

#include "FrameSource.hpp"

#include <mjpegproxy/Endpoint.hpp>

#include <chrono>

namespace mjpegproxy {

/// ESP32 con firmware standard: GET http://host:81/stream (multipart, chunked).
class HttpMjpegSource final : public FrameSource {
public:
    explicit HttpMjpegSource(Endpoint endpoint, std::chrono::seconds io_timeout = std::chrono::seconds(5));

    std::string describe() const override;
    asio::awaitable<void> stream(FrameSink& sink, std::function<bool()> keep_going) override;

private:
    Endpoint endpoint_;
    std::chrono::seconds timeout_;
};

}  // namespace mjpegproxy
