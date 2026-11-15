#include <mjpegproxy/Server.hpp>

#include "Asio.hpp"
#include "CameraRegistry.hpp"
#include "HttpServer.hpp"
#include "Log.hpp"

#include <csignal>

namespace mjpegproxy {

/// Tutta l'implementazione (e tutto Boost) vive qui, fuori dagli header pubblici.
class Server::Impl {
public:
    explicit Impl(const Settings& settings)
        : registry_(CameraRegistry::from_specs(settings.cameras, io_.get_executor())),
          http_(io_.get_executor(), registry_, {asio::ip::make_address(settings.bind), settings.port},
                ServerLimits{.max_clients_per_camera = settings.max_clients_per_camera,
                             .max_connections        = settings.max_connections}),
          signals_(io_, SIGINT, SIGTERM) {
        Log::info("server", std::to_string(settings.cameras.size()) + " camere configurate");
    }

    void run() {
        registry_.start_all(io_.get_executor());
        asio::co_spawn(io_, http_.accept_loop(), asio::detached);
        signals_.async_wait([this](const boost::system::error_code&, int signal) {
            Log::info("server", "segnale " + std::to_string(signal) + ": arresto");
            io_.stop();
        });
        io_.run();
    }

    void stop() {
        asio::post(io_, [this] { io_.stop(); });
    }

private:
    // L'ordine conta: l'io_context e' il primo membro, quindi l'ultimo a essere distrutto.
    // Le coroutine ancora in sospeso vengono distrutte con lui; tengono in vita le
    // camere con uno shared_ptr, quindi non toccano oggetti gia' distrutti.
    asio::io_context io_{1};
    CameraRegistry registry_;
    HttpServer http_;
    asio::signal_set signals_;
};

Server::Server(const Settings& settings) : impl_(std::make_unique<Impl>(settings)) {}
Server::~Server() = default;
void Server::run() { impl_->run(); }
void Server::stop() { impl_->stop(); }

}  // namespace mjpegproxy
