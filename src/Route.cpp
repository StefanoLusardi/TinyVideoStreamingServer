#include "Route.hpp"

#include <mjpegproxy/CameraSpec.hpp>

namespace mjpegproxy {

Route Route::parse(std::string_view target) {
    target = target.substr(0, target.find('?'));
    if (target == "/") return {Kind::Index, {}};
    if (target == "/healthz") return {Kind::Health, {}};
    if (!target.starts_with('/')) return {};
    target.remove_prefix(1);

    const auto slash = target.find('/');
    const auto name  = target.substr(0, slash);
    if (!CameraSpec::is_valid_name(name)) return {};

    Route route{Kind::NotFound, std::string(name)};
    const auto rest = slash == std::string_view::npos ? std::string_view{} : target.substr(slash + 1);
    if (rest.empty()) route.kind = Kind::CameraPage;
    else if (rest == "stream" || rest == "stream.mjpeg") route.kind = Kind::Stream;
    else if (rest == "snapshot.jpg") route.kind = Kind::Snapshot;
    return route;
}

}  // namespace mjpegproxy
