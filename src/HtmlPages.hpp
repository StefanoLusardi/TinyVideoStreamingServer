#pragma once

#include <string>
#include <vector>

namespace mjpegproxy {

/// Le pagine HTML del server.
class HtmlPages {
public:
    /// Griglia con le camere indicate. I nomi sono gia' validati ([A-Za-z0-9_-]):
    /// non serve alcun escaping.
    static std::string index(const std::vector<std::string>& camera_names);
};

}  // namespace mjpegproxy
