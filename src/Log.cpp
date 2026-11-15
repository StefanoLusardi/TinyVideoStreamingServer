#include "Log.hpp"

#include <cstdio>
#include <ctime>

namespace mjpegproxy {

void Log::info(std::string_view who, std::string_view msg) { write("INFO", who, msg); }
void Log::warn(std::string_view who, std::string_view msg) { write("WARN", who, msg); }
void Log::error(std::string_view who, std::string_view msg) { write("ERROR", who, msg); }

void Log::write(const char* level, std::string_view who, std::string_view msg) {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &tm);
    std::fprintf(stderr, "%s %-5s [%.*s] %.*s\n", stamp, level, static_cast<int>(who.size()), who.data(),
                 static_cast<int>(msg.size()), msg.data());
}

}  // namespace mjpegproxy
