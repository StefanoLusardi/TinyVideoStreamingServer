#include <mjpegproxy/CameraSpec.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace mjpegproxy {

bool CameraSpec::is_valid_name(std::string_view name) {
    return !name.empty() && name.size() <= 32 &&
           std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

CameraSpec CameraSpec::parse(std::string_view arg) {
    std::string name = "cam";
    if (!arg.starts_with("http://")) {
        const auto eq = arg.find('=');
        if (eq == std::string_view::npos)
            throw std::invalid_argument("camera '" + std::string(arg) + "': atteso nome=http://host[:porta]/path");
        name = std::string(arg.substr(0, eq));
        arg.remove_prefix(eq + 1);
    }
    if (!is_valid_name(name))
        throw std::invalid_argument("nome camera non valido '" + name + "' (ammessi: lettere, cifre, - _ ; max 32)");
    return CameraSpec{std::move(name), Endpoint::parse(arg)};
}

}  // namespace mjpegproxy
