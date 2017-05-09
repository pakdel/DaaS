// TODO test batching

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "net_api_mock.hpp"
#include "redirect_stdout.hpp"

//#include "logging_test.hpp"

#include "datadog.hpp"
#include "datadog.tpp"

typedef DaaS::metrics::Datadog<net_api_mock> metric_handler;

using namespace DaaS;

TEST_F(RedirectSTDOUT, Datadog) {
  auto metrics_queue = std::make_shared<metric_handler>(
      "localhost.localdomain", "http", "00000000-OOOO-0000-OOOO-000000000000");

  metrics_queue->push(metrics::MetricName::request_queue_size, 0);
  //  metrics_queue.reset();
  metrics_queue->flush();

  //  ASSERT_STREQ(
  //      buffer.str().c_str(),
  ASSERT_THAT(buffer.str().c_str(),
              ::testing::StartsWith(
                  "endpoint: localhost.localdomain:http\n"
                  "send POST "
                  "/api/v1/series?api_key=00000000-OOOO-0000-OOOO-000000000000 "
                  "HTTP/1.1\r\n"
                  "Host: localhost.localdomain\r\n"
                  "Content-type: application/json\r\n"
                  "Accept: */*\r\n"
                  "Connection: close\r\n"
                  "Content-Length: 150\r\n"
                  "\r\n"
                  "{\"series\":[ "
                  "{\"metric\":\"DaaS.request_queue_size\","
                  "\"type\":\"gauge\","
                  "\"host\":\"" METRIC_SOURCE_HOST "\","
                  "\"tags\":[" METRIC_TAGS "],"
                  "\"points\":[ ["));

  std::string output_ending = ",0]]"
                              "}"
                              "]}\n"
                              "Mocking receive\n";
#ifndef NDEBUG
  output_ending += "metric sent\n";
#endif // NDEBUG
  ASSERT_THAT(buffer.str().c_str(), ::testing::EndsWith(output_ending));
}
