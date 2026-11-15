#pragma once

#include <string_view>

namespace mjpegproxy {

/// Log su stderr, una riga per messaggio: "<UTC> <livello> [<chi>] <messaggio>".
class Log {
public:
    static void info(std::string_view who, std::string_view msg);
    static void warn(std::string_view who, std::string_view msg);
    static void error(std::string_view who, std::string_view msg);

private:
    static void write(const char* level, std::string_view who, std::string_view msg);
};

}  // namespace mjpegproxy
