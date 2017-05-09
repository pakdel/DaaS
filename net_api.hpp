// FIXME boost::system::system_error Connection timed out
// TODO Use a single io_service
// TODO Do we need to verify SSL Certificate of Datadog?
// TODO Do we need to verify SSL Certificate of Logentries?
// TODO if constexpr

#ifndef NET_API_HPP
#define NET_API_HPP

#include "DaaS_config.h"

#include <boost/asio/use_future.hpp>
#include <iostream>

namespace ssl = boost::asio::ssl;

namespace DaaS {

class NetAPI {
private:
#ifndef NDEBUG
  std::string _id;
#endif // NDEBUG
  boost::asio::io_service _io_service;
  boost::asio::io_service::work _work;
  std::thread _thread;

  typedef boost::asio::ip::tcp tcp;
  typedef ssl::stream<tcp::socket> ssl_stream;
  tcp::resolver _resolver;
  tcp::resolver::query _query;
  ssl::context _ctx;
  ssl_stream _socket_stream;

public:
  NetAPI() = delete;
  NetAPI(std::string host, std::string port)
      :
#ifndef NDEBUG
        _id(host + ":" + port),
#endif // NDEBUG
        _work(_io_service), _resolver(_io_service), _query(host, port),
        _ctx(ssl::context::sslv23), _socket_stream(_io_service, _ctx) {
    _thread = std::thread([&] { _io_service.run(); });
    // ctx.set_verify_mode(ssl::verify_peer);
    _ctx.set_verify_mode(ssl::verify_none);
  }
  ~NetAPI();
  void close();

  bool send(const boost::asio::const_buffers_1 &data);

  std::string receive();
};

} // namespace DaaS

#endif // NET_API_HPP
