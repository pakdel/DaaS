// TODO Do not "Connection: close"

#ifndef DATADOG_HPP
#define DATADOG_HPP

#include "DaaS_config.h"

#include "logging.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <iostream>
#include <istream>
#include <json/json.h>
#include <ostream>
#include <string>
#include <thread>

namespace DaaS::metrics {

enum MetricName {
  request_queue_size,
  server_overhead_time, // _t_queued - _t_create
  wait_time,            // _t_grabbed - _t_queued
  process_time,         // now - _t_grabbed
  total_response_time   // now - _t_created
  /*, logs_processed, log_queue_size */,
  metric_count
};
static const char *MetricNameString[static_cast<int>(MetricName::metric_count)]{
    "DaaS.request_queue_size", "DaaS.server_overhead_time", "DaaS.wait_time",
    "DaaS.process_time", "DaaS.total_response_time"
    /*, "logs_processed", "log_queue_size"*/
};

struct MetricDataPoint {
public:
  std::chrono::seconds::rep _t;
  unsigned int _v;
  explicit MetricDataPoint(unsigned int value) : _t(EPOCH()), _v(value) {}
  MetricDataPoint() = delete;
};

template <typename net_api> class Datadog {
private:
  // It needs to ne initialized early, because it is used everywhere
  log_handler &_logger;
  net_api _net_api;
  std::string _request_header;
  std::atomic_bool _running;
  std::thread _datadog_thread;

  std::vector<MetricDataPoint>
      _metric_queue[static_cast<int>(MetricName::metric_count)];
  unsigned int _metric_queue_size;
  std::mutex _m;
  std::condition_variable _cv;

#ifndef NDEBUG
  unsigned int metrics_processed{0};
  unsigned int metrics_series_sent{0};
#endif // NDEBUG

  void process_metrics_thread();

public:
  explicit Datadog(std::string host, std::string port, std::string DD_API_KEY);
  Datadog() = delete;
  Datadog(Datadog const &) = delete;
  Datadog(Datadog &) = delete;                  // non construction-copyable
  Datadog(Datadog &&) = delete;                 // non construction-movable
  Datadog &operator=(const Datadog &) = delete; // non copy assignable
  Datadog &operator=(Datadog &&) = delete;      // non move assignable
  ~Datadog();
#ifdef TEST
  inline void flush() {
    while (_running && _metric_queue_size > 0) {
      if (_metric_queue_size > 0)
        _cv.notify_one();
      std::this_thread::sleep_for(100ms);
    }
  }
#endif

  void push(const MetricName name, const unsigned long int value);
};

} // namespace DaaS::metrics

#endif // DATADOG_HPP
