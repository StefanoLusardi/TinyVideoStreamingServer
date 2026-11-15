#pragma once

#include "Asio.hpp"
#include "Camera.hpp"

#include <mjpegproxy/CameraSpec.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mjpegproxy {

/// Le camere configurate, cercabili per nome.
class CameraRegistry {
public:
    /// Una Camera (con origine HttpMjpegSource) per ogni specifica.
    static CameraRegistry from_specs(const std::vector<CameraSpec>& specs, const asio::any_io_executor& ex);

    /// Lancia std::invalid_argument se il nome esiste gia'.
    void add(std::shared_ptr<Camera> camera);

    /// nullptr se non esiste.
    std::shared_ptr<Camera> find(std::string_view name) const;

    /// Nomi nell'ordine di configurazione.
    const std::vector<std::string>& names() const;

    /// Avvia il supervisore di ogni camera.
    void start_all(const asio::any_io_executor& ex) const;

private:
    std::map<std::string, std::shared_ptr<Camera>, std::less<>> by_name_;
    std::vector<std::string> order_;
};

}  // namespace mjpegproxy
