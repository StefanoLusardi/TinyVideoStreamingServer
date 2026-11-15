#pragma once

#include "Asio.hpp"
#include "Frame.hpp"
#include "FrameSink.hpp"

#include <cstddef>
#include <cstdint>
#include <set>

namespace mjpegproxy {

/// Ultimo frame di una camera + fan-out verso i client (Subscriber).
/// Pensato per un solo thread (quello dell'io_context): nessun lock.
class FrameHub final : public FrameSink {
public:
    explicit FrameHub(asio::any_io_executor ex);
    FrameHub(const FrameHub&)            = delete;
    FrameHub& operator=(const FrameHub&) = delete;

    /// Un client interessato ai frame. Finche' esiste, la camera resta connessa.
    class Subscriber {
    public:
        explicit Subscriber(FrameHub& hub);
        ~Subscriber();
        Subscriber(const Subscriber&)            = delete;
        Subscriber& operator=(const Subscriber&) = delete;

        void notify();

        /// Attende un frame con seq > last_seq. nullptr se scade il timeout.
        asio::awaitable<FramePtr> next(std::uint64_t last_seq, Clock::duration timeout);

    private:
        FrameHub& hub_;
        asio::steady_timer wake_;
    };

    void publish(std::string jpeg) override;

    FramePtr latest() const;
    std::size_t subscriber_count() const;

    /// Ritorna appena c'e' almeno uno spettatore.
    asio::awaitable<void> wait_for_subscribers();

private:
    asio::any_io_executor ex_;
    asio::steady_timer idle_;
    std::set<Subscriber*> subs_;
    FramePtr latest_;
    std::uint64_t seq_ = 0;
};

}  // namespace mjpegproxy
