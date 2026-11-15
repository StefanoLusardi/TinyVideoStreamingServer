#include "FrameHub.hpp"

namespace mjpegproxy {

FrameHub::FrameHub(asio::any_io_executor ex) : ex_(std::move(ex)), idle_(ex_, Clock::time_point::max()) {}

FrameHub::Subscriber::Subscriber(FrameHub& hub) : hub_(hub), wake_(hub.ex_, Clock::time_point::max()) {
    hub_.subs_.insert(this);
    hub_.idle_.cancel();  // sveglia chi attende il primo spettatore
}

FrameHub::Subscriber::~Subscriber() { hub_.subs_.erase(this); }

void FrameHub::Subscriber::notify() { wake_.cancel(); }

asio::awaitable<FramePtr> FrameHub::Subscriber::next(std::uint64_t last_seq, Clock::duration timeout) {
    const auto deadline = Clock::now() + timeout;
    for (;;) {
        if (auto f = hub_.latest_; f && f->seq > last_seq) co_return f;
        if (Clock::now() >= deadline) co_return nullptr;
        wake_.expires_at(deadline);
        co_await wake_.async_wait(kNoErr);  // cancel() = nuovo frame; scadenza = timeout
    }
}

void FrameHub::publish(std::string jpeg) {
    latest_ = std::make_shared<const Frame>(Frame{std::move(jpeg), ++seq_, Clock::now()});
    for (auto* s : subs_) s->notify();
}

FramePtr FrameHub::latest() const { return latest_; }

std::size_t FrameHub::subscriber_count() const { return subs_.size(); }

asio::awaitable<void> FrameHub::wait_for_subscribers() {
    while (subs_.empty()) {
        idle_.expires_at(Clock::time_point::max());
        co_await idle_.async_wait(kNoErr);
    }
}

}  // namespace mjpegproxy
