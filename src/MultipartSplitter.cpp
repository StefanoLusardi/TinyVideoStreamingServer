#include "MultipartSplitter.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <stdexcept>

namespace mjpegproxy {

namespace {

std::string to_lower(std::string_view s) {
    std::string r(s);
    std::ranges::transform(r, r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::optional<std::size_t> content_length(std::string_view headers) {
    const auto lower = to_lower(headers);
    auto p           = lower.find("content-length:");
    if (p == std::string::npos) return std::nullopt;
    p += 15;
    while (p < lower.size() && lower[p] == ' ') ++p;
    std::size_t v        = 0;
    const auto [end, ec] = std::from_chars(lower.data() + p, lower.data() + lower.size(), v);
    if (ec != std::errc{} || end == lower.data() + p || v > kMaxFrameBuffer) return std::nullopt;
    return v;
}

// SOI JPEG: FF D8. Scarta le parti che non sono JPEG.
void emit(std::string jpeg, const MultipartSplitter::FrameCallback& on_frame) {
    if (jpeg.size() > 4 && static_cast<unsigned char>(jpeg[0]) == 0xFF && static_cast<unsigned char>(jpeg[1]) == 0xD8)
        on_frame(std::move(jpeg));
}

}  // namespace

MultipartSplitter::MultipartSplitter(std::string boundary) : delim_("--" + std::move(boundary)) {}

std::string MultipartSplitter::boundary_from(std::string_view content_type) {
    const auto lower = to_lower(content_type);
    const auto p     = lower.find("boundary=");
    if (p == std::string::npos) throw std::runtime_error("la camera non risponde con un multipart");
    auto v = content_type.substr(p + 9);
    if (const auto e = v.find(';'); e != std::string_view::npos) v = v.substr(0, e);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return std::string(v);
}

void MultipartSplitter::feed(std::string_view data, const FrameCallback& on_frame) {
    buf_.append(data);
    for (;;) {
        const auto d = buf_.find(delim_);
        if (d == std::string::npos) return guard();
        if (d > 0) buf_.erase(0, d);  // scarta quel che precede il delimitatore

        const auto h = buf_.find("\r\n\r\n");
        if (h == std::string::npos) return guard();
        const std::size_t body = h + 4;
        const std::string_view headers(buf_.data() + delim_.size(), h - delim_.size());

        if (const auto len = content_length(headers)) {
            if (buf_.size() < body + *len) return guard();
            emit(buf_.substr(body, *len), on_frame);
            buf_.erase(0, body + *len);
        } else {
            const auto n = buf_.find(delim_, body);
            if (n == std::string::npos) return guard();
            std::size_t end = n;
            if (end >= body + 2 && buf_.compare(end - 2, 2, "\r\n") == 0) end -= 2;
            emit(buf_.substr(body, end - body), on_frame);
            buf_.erase(0, n);
        }
    }
}

void MultipartSplitter::guard() {
    if (buf_.size() > kMaxFrameBuffer) buf_.clear();  // stream corrotto: risincronizza
}

}  // namespace mjpegproxy
