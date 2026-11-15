#include "Camera.hpp"

#include "Log.hpp"

#include <exception>

namespace mjpegproxy {

Camera::Camera(std::string name, asio::any_io_executor ex, std::unique_ptr<FrameSource> source, CameraOptions options)
    : name_(std::move(name)), ex_(ex), hub_(ex), source_(std::move(source)), options_(options) {}

const std::string& Camera::name() const { return name_; }

FrameHub& Camera::hub() { return hub_; }

asio::awaitable<void> Camera::supervise(std::shared_ptr<Camera> self) {
    for (;;) {
        co_await self->hub_.wait_for_subscribers();
        bool failed = false;
        try {
            Log::info(self->name_, "connessione a " + self->source_->describe());
            FrameHub& hub = self->hub_;
            co_await self->source_->stream(hub, [&hub] { return hub.subscriber_count() > 0; });
            Log::info(self->name_, "nessuno spettatore: connessione chiusa");
        } catch (const boost::system::system_error& e) {  // niente co_await dentro catch
            Log::warn(self->name_, e.code().message() + " (riprovo)");
            failed = true;
        } catch (const std::exception& e) {
            Log::warn(self->name_, std::string(e.what()) + " (riprovo)");
            failed = true;
        }
        if (failed) {
            asio::steady_timer backoff(self->ex_, self->options_.retry_delay);
            co_await backoff.async_wait(kNoErr);
        }
    }
}

}  // namespace mjpegproxy
