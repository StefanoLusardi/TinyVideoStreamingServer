#include "HtmlPages.hpp"

namespace mjpegproxy {

std::string HtmlPages::index(const std::vector<std::string>& camera_names) {
    std::string html =
        "<!doctype html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>Camere</title><style>body{margin:0;background:#111;color:#ccc;font:14px system-ui}"
        "main{display:grid;gap:8px;padding:8px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}"
        "figure{margin:0}img{width:100%;display:block;background:#000}a{color:inherit}figcaption{padding:4px 0}"
        "</style><main>";
    for (const auto& n : camera_names)
        html += "<figure><a href=\"/" + n + "/\"><img src=\"/" + n + "/stream\" alt=\"" + n + "\"></a><figcaption>" + n +
                "</figcaption></figure>";
    return html + "</main>";
}

}  // namespace mjpegproxy
