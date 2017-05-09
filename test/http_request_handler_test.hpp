#include "http_request_handler.hpp"
#include "gtest/gtest.h"

#include <string>

typedef nghttp2::asio_http2::server::request http_request;
typedef nghttp2::asio_http2::server::response http_response;

#include <iostream>
class response_mock : http_response {

  // Write response header using |status_code| (e.g., 200) and
  // additional header fields in |h|.
  void write_head(
      unsigned int status_code,
      nghttp2::asio_http2::header_map h = nghttp2::asio_http2::header_map{}) {
    std::cerr << "Status Code: " << status_code << std::endl;
  }

  // Sends |data| as request body.  No further call of end() is
  // allowed.
  void end(std::string data = "") {
    std::cerr << "Data: " << data << std::endl;
  }

  // Cancels this request and response with given error code.
  void cancel(uint32_t error_code = NGHTTP2_INTERNAL_ERROR) {
    std::cerr << "Error Code: " << error_code << std::endl;
  }
};

TEST(RequestHandler, HTTP) {

  auto metrics_queue = std::make_shared<metric_handler>(
      "localhost.localdomain", "http", "00000000-OOOO-0000-OOOO-000000000000");

  auto request_queue = std::unique_ptr<http_request_handler>(
      new http_request_handler(2, metrics_queue));

  http_request req;
  http_response res;
  request_queue->push(req, res);
  std::this_thread::sleep_for(950ms);
  //  ASSERT_EQ(nullptr, datum);
}
