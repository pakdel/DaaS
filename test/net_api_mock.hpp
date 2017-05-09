#ifndef NET_API_MOCK_HPP
#define NET_API_MOCK_HPP

#include "gtest/gtest.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include <string>

class net_api_mock {
public:
  net_api_mock(std::string host, std::string port) {
    std::cout << "endpoint: " << host << ":" << port << "\n";
  }
  //  ~my_net_api() { std::cout << "~my_net_api" << std::endl; }
  bool send(const boost::asio::const_buffers_1 &data) {
    std::cout << "send " << std::string(boost::asio::buffers_begin(data),
                                        boost::asio::buffers_end(data));
    return true;
  }

  std::string receive() {
    std::cout << "\nMocking receive\n";
    return "HTTP/1.1 202 Accepted\r\n"
           "Content-Type: text/json\r\n"
           "Date: Day, 00 Mon Year 00:00:00 GMT\r\n"
           "DD-POOL: propjoe\r\n"
           "Strict-Transport-Security: max-age=15724800;\r\n"
           "X-Content-Type-Options: nosniff\r\n"
           "Content-Length: 16\r\n"
           "Connection: Close\r\n"
           "\r\n"
           "{\"status\": \"ok\"}";
  }
  void close() {}
};

#endif // NET_API_MOCK_HPP
