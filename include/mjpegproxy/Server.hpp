#pragma once

#include <mjpegproxy/Settings.hpp>

#include <memory>

namespace mjpegproxy {

/// Server HTTP che ri-trasmette lo stream MJPEG delle camere configurate.
///
/// Questa e' l'unica API pubblica che "fa" qualcosa; non espone tipi di Boost
/// (l'implementazione e' nascosta dietro un puntatore opaco).
class Server {
public:
    /// Prepara le camere e apre la porta di ascolto.
    /// Lancia un'eccezione se la porta non e' disponibile.
    explicit Server(const Settings& settings);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    /// Serve le richieste. Blocca finche' non arriva SIGINT/SIGTERM o stop().
    /// Da chiamare una sola volta.
    void run();

    /// Interrompe run(). Puo' essere chiamato da un altro thread.
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mjpegproxy
