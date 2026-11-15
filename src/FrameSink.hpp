#pragma once

#include <string>

namespace mjpegproxy {

/// Dove una FrameSource deposita i frame.
class FrameSink {
public:
    virtual ~FrameSink()                   = default;
    virtual void publish(std::string jpeg) = 0;
};

}  // namespace mjpegproxy
