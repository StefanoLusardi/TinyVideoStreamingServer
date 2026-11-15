#include "HttpMjpegSource.hpp"

#include "MultipartSplitter.hpp"

#include <boost/beast.hpp>

#include <array>
#include <stdexcept>

namespace mjpegproxy {

namespace beast = boost::beast;
namespace http  = beast::http;

HttpMjpegSource::HttpMjpegSource(Endpoint endpoint, std::chrono::seconds io_timeout)
    : endpoint_(std::move(endpoint)), timeout_(io_timeout) {}

std::string HttpMjpegSource::describe() const { return endpoint_.host + ":" + endpoint_.port + endpoint_.target; }

asio::awaitable<void> HttpMjpegSource::stream(FrameSink& sink, std::function<bool()> keep_going) {
    auto ex = co_await asio::this_coro::executor;

    tcp::resolver resolver(ex);
    const auto endpoints = co_await resolver.async_resolve(endpoint_.host, endpoint_.port, asio::use_awaitable);

    beast::tcp_stream conn(ex);
    conn.expires_after(timeout_);
    co_await conn.async_connect(endpoints, asio::use_awaitable);

    http::request<http::empty_body> req{http::verb::get, endpoint_.target, 11};
    req.set(http::field::host, endpoint_.host);
    req.set(http::field::user_agent, "mjpeg-proxy");
    conn.expires_after(timeout_);
    co_await http::async_write(conn, req, asio::use_awaitable);

    beast::flat_buffer buf;
    http::response_parser<http::buffer_body> parser;
    parser.body_limit(boost::none);  // stream infinito
    conn.expires_after(timeout_);
    co_await http::async_read_header(conn, buf, parser, asio::use_awaitable);

    if (parser.get().result() != http::status::ok)
        throw std::runtime_error("la camera ha risposto " + std::to_string(parser.get().result_int()));

    const auto ct = parser.get()[http::field::content_type];
    MultipartSplitter splitter(MultipartSplitter::boundary_from({ct.data(), ct.size()}));

    std::array<char, 16 * 1024> chunk;
    while (keep_going()) {
        auto& body = parser.get().body();
        body.data  = chunk.data();
        body.size  = chunk.size();

        conn.expires_after(timeout_);  // niente dati per timeout_ => errore => riconnessione
        auto [ec, n] = co_await http::async_read_some(conn, buf, parser, kNoErr);
        if (ec && ec != http::error::need_buffer) throw beast::system_error(ec);

        splitter.feed({chunk.data(), chunk.size() - body.size}, [&](std::string jpeg) { sink.publish(std::move(jpeg)); });
        if (parser.is_done()) throw std::runtime_error("la camera ha chiuso lo stream");
    }
}

}  // namespace mjpegproxy
