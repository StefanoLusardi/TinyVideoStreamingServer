#include "CameraRegistry.hpp"

#include "HttpMjpegSource.hpp"

#include <stdexcept>

namespace mjpegproxy {

CameraRegistry CameraRegistry::from_specs(const std::vector<CameraSpec>& specs, const asio::any_io_executor& ex) {
    CameraRegistry registry;
    for (const auto& spec : specs)
        registry.add(std::make_shared<Camera>(spec.name, ex, std::make_unique<HttpMjpegSource>(spec.endpoint)));
    return registry;
}

void CameraRegistry::add(std::shared_ptr<Camera> camera) {
    const std::string name = camera->name();
    if (!by_name_.emplace(name, std::move(camera)).second) throw std::invalid_argument("camera duplicata: " + name);
    order_.push_back(name);
}

std::shared_ptr<Camera> CameraRegistry::find(std::string_view name) const {
    const auto it = by_name_.find(name);
    return it == by_name_.end() ? nullptr : it->second;
}

const std::vector<std::string>& CameraRegistry::names() const { return order_; }

void CameraRegistry::start_all(const asio::any_io_executor& ex) const {
    for (const auto& [name, camera] : by_name_) asio::co_spawn(ex, Camera::supervise(camera), asio::detached);
}

}  // namespace mjpegproxy
