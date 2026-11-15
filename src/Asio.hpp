#pragma once

// Header INTERNO: l'unico punto in cui il progetto sceglie come includere Boost.Asio.
// Non va mai incluso dagli header pubblici (include/mjpegproxy/).

#include <boost/asio.hpp>

#include <chrono>

namespace mjpegproxy {

namespace asio = boost::asio;
using asio::ip::tcp;
using Clock = std::chrono::steady_clock;

/// co_await che restituisce {error_code, ...} invece di lanciare un'eccezione.
inline constexpr auto kNoErr = asio::as_tuple(asio::use_awaitable);

}  // namespace mjpegproxy
