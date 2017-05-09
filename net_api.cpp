// FIXME handle boost::system::system_error exceptions

#include "net_api.hpp"

namespace DaaS {
bool NetAPI::send(const boost::asio::const_buffers_1 &data) {
  // FIXME Might be open, but not connected
  if (!_socket_stream.lowest_layer().is_open()) {
    auto resolve_result =
        _resolver.async_resolve(_query, boost::asio::use_future);
    if (resolve_result.wait_for(NET_API_TIMEOUT) == std::future_status::timeout)
      return false;

    auto connect_result = boost::asio::async_connect(
        _socket_stream.lowest_layer(), resolve_result.get(),
        boost::asio::use_future);
    if (connect_result.wait_for(NET_API_TIMEOUT) == std::future_status::timeout)
      return false;
    // throws boost::system::system_error if the operation fails
    connect_result.get();

    auto handshake_result = _socket_stream.async_handshake(
        ssl_stream::client, boost::asio::use_future);
    if (handshake_result.wait_for(NET_API_TIMEOUT) ==
        std::future_status::timeout)
      return false;
    handshake_result.get();
  }

  assert(_socket_stream.lowest_layer().is_open());

  // If we want to handle string instead of const_buffers_1
  //  auto write_result = boost::asio::async_write(
  //      _socket_stream, boost::asio::buffer(data), boost::asio::use_future);
  auto write_result =
      boost::asio::async_write(_socket_stream, data, boost::asio::use_future);
  if (write_result.wait_for(NET_API_TIMEOUT) == std::future_status::timeout)
    return false;

  write_result.get();

  return true;
}

std::string NetAPI::receive() {
  boost::asio::streambuf response;
  auto read_result = boost::asio::async_read(
      //        _socket_stream, response, boost::asio::transfer_exactly(239),
      //        _socket_stream, response, boost::asio::transfer_at_least(239),
      _socket_stream, response, boost::asio::transfer_all(),
      boost::asio::use_future);

  if (read_result.wait_for(NET_API_TIMEOUT) == std::future_status::timeout)
    return "Timed out!";

  try {
    read_result.get();
  } catch (const boost::system::system_error &e) {
    // FIXME Do we need to close?
    //    close();
    if (e.code() != boost::asio::error::eof)
      return e.what();
  }

  return std::string(std::istreambuf_iterator<char>(&response),
                     std::istreambuf_iterator<char>());
}

void NetAPI::close() {
  if (!_socket_stream.lowest_layer().is_open())
    return;
  boost::system::error_code ec;
  _socket_stream.lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
#ifndef NDEBUG
  if (ec && ec != boost::asio::error::basic_errors::not_connected) {
    std::cerr << "Failed to shutdown connection to " << _id << ": "
              << ec.message() << " (error code " << ec.value() << ")"
              << std::endl;
  }
#endif // NDEBUG
  _socket_stream.lowest_layer().close();
}

NetAPI::~NetAPI() {
  close();
  _io_service.stop();
  assert(_thread.joinable());
  _thread.join();
#ifndef NDEBUG
  std::cout << "............................................................"
            << std::endl
            << "~NetAPI" << std::endl
            << "............................................................"
            << std::endl;
#endif // NDEBUG
}

} // namespace DaaS
