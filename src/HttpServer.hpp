#pragma once

#include "Asio.hpp"
#include "CameraRegistry.hpp"

#include <boost/beast/core/tcp_stream.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>

namespace mjpegproxy {

struct ServerLimits {
    std::size_t max_clients_per_camera = 32;
    std::size_t max_connections        = 256;
    std::chrono::seconds request_timeout{10};
    std::chrono::seconds write_timeout{5};  // client lento o morto => lo scarto
};

/// Lato client: accetta connessioni HTTP e le serve (pagine, stream, snapshot).
class HttpServer {
public:
    /// Apre subito la porta: se non e' possibile lancia boost::system::system_error.
    HttpServer(asio::any_io_executor ex, CameraRegistry& cameras, const tcp::endpoint& listen_on, ServerLimits limits = {});

    /// Accetta connessioni all'infinito (da lanciare con co_spawn).
    asio::awaitable<void> accept_loop();

    /// Una connessione client (pubblica: i test possono passare un socket gia' connesso).
    asio::awaitable<void> serve(tcp::socket sock);

private:
    asio::awaitable<void> serve_counted(tcp::socket sock, std::shared_ptr<std::size_t> counter);
    asio::awaitable<void> send_simple(boost::beast::tcp_stream& s, std::string_view status, std::string_view type,
                                      std::string_view body);
    asio::awaitable<void> stream_to(boost::beast::tcp_stream& s, std::shared_ptr<Camera> camera);
    asio::awaitable<void> snapshot_to(boost::beast::tcp_stream& s, std::shared_ptr<Camera> camera);

    asio::any_io_executor ex_;
    CameraRegistry& cameras_;
    ServerLimits limits_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::size_t> active_ = std::make_shared<std::size_t>(0);  // connessioni in corso
};

}  // namespace mjpegproxy
