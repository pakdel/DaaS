#include "datadog.hpp"

namespace DaaS::metrics {

template <typename net_api>
Datadog<net_api>::Datadog(std::string host, std::string port,
                          std::string DD_API_KEY)
    : _net_api(host, port), _logger(log_handler::getInstance()), _running(true),
      _metric_queue_size(0),
      _datadog_thread(&Datadog::process_metrics_thread, this) {

  // Assuming a uniform distribution
  unsigned int reserved_capacity =
      METRIC_MAX_QUEUE_ZISE / static_cast<int>(MetricName::metric_count) + 1;
  for (int i = 0; i < static_cast<int>(MetricName::metric_count); ++i)
    _metric_queue[i].reserve(reserved_capacity);
  // In the request, we specify the "Connection: close" header so that the
  // server will close the socket after transmitting the response. This will
  // allow us to treat all data up until the EOF as the content.
  std::ostringstream oss;
  oss << "POST /api/v1/series?api_key=" << DD_API_KEY << " HTTP/1.1\r\n";
  oss << "Host: " << host << "\r\n"
                             "Content-type: application/json\r\n"
                             "Accept: */*\r\n"
                             "Connection: close\r\n";
  _request_header = oss.str();
}

template <typename net_api> void Datadog<net_api>::process_metrics_thread() {
  _logger.debug("Starting process_metric_thread...");

  while (_running || _metric_queue_size > 0) {
    std::unique_lock<std::mutex> lck{_m};
    auto deadline = NOW() + METRIC_SLEEP;
    while (_running && _metric_queue_size < METRIC_MAX_QUEUE_ZISE)
      if (_cv.wait_until(lck, deadline) == std::cv_status::timeout &&
          _metric_queue_size > 0)
        break;
    // Mutex is aquired by _cv.wait

    if (/* !_running && */ _metric_queue_size == 0) {
      assert(!_running);
      // Might not be needed!
      lck.unlock();
      break;
    }

    std::ostringstream json_oss;
    json_oss << "{\"series\":[";
#ifndef NDEBUG
    unsigned int metrics_count = 0;
#endif // NDEBUG
    char metric_separator = ' ';
    for (int i = 0; i < static_cast<int>(MetricName::metric_count); ++i) {
      if (_metric_queue[i].empty())
        continue;
      json_oss << metric_separator << "{\"metric\":\"" << MetricNameString[i]
               << "\","
                  "\"type\":\"gauge\","
                  "\"host\":\"" METRIC_SOURCE_HOST "\","
                  "\"tags\":[" METRIC_TAGS "],"
                  "\"points\":[";

      char point_separator = ' ';
      for (auto const &tmp_metric : _metric_queue[i]) {
        json_oss << point_separator << "[" << tmp_metric._t << ","
                 << tmp_metric._v << "]";
        point_separator = ',';
#ifndef NDEBUG
        ++metrics_count;
#endif // NDEBUG
      }
      _metric_queue[i].clear();
      json_oss << "]}";
      metric_separator = ',';
    }
#ifndef NDEBUG
    assert(metrics_count == _metric_queue_size);
#endif // NDEBUG
    _metric_queue_size = 0;
    lck.unlock();

    json_oss << "]}";

    std::string json = json_oss.str();
    _logger.verbose("JSON: ", json);

    boost::asio::streambuf msg;
    std::ostream msg_stream(&msg);
    msg_stream << _request_header << "Content-Length: " << json.length()
               << "\r\n\r\n"
               << json;
    if (!_net_api.send(msg.data()))
      _logger.error(logging::Error::datadog_net, "Could not send metric JSON ",
                    json);

#ifndef NDEBUG
    metrics_processed += metrics_count;
#endif // NDEBUG

    // Check that response is OK.
    /*
    HTTP/1.1 202 Accepted
    Content-Type: text/json
    Date: Wed, 25 Jan 2017 04:00:26 GMT
    DD-POOL: propjoe
    Strict-Transport-Security: max-age=15724800;
    X-Content-Type-Options: nosniff
    Content-Length: 16
    Connection: Close

    {"status": "ok"}
    */

    auto response = _net_api.receive();
    _net_api.close();
    if (response == "Timed out!") {
      _logger.error(logging::Error::datadog_net,
                    "Could not receive the response ", json);
      continue;
    }
    if (response.length() != 239) {
      _logger.warn(logging::Warning::datadog_wrong_size, response.length(),
                   ": ", response);
      continue;
    }
    response.replace(54, 25, "Day, 00 Mon Year 00:00:00");
    if (response != "HTTP/1.1 202 Accepted\r\n"
                    "Content-Type: text/json\r\n"
                    "Date: Day, 00 Mon Year 00:00:00 GMT\r\n"
                    "DD-POOL: propjoe\r\n"
                    "Strict-Transport-Security: max-age=15724800;\r\n"
                    "X-Content-Type-Options: nosniff\r\n"
                    "Content-Length: 16\r\n"
                    "Connection: Close\r\n"
                    "\r\n"
                    "{\"status\": \"ok\"}") {
      _logger.warn(logging::Warning::datadog_invalid_response, response);
      continue;
    }
#ifndef NDEBUG
    ++metrics_series_sent;
  }
  _logger.log("Processed metrics: ", metrics_processed,
              "\n\tSeries sent successfully: ", metrics_series_sent);
#else
  }
#endif // NDEBUG
       // Unnecessary
  // _net_api.close();
}

template <typename net_api> Datadog<net_api>::~Datadog() {
  _running = false;
  _cv.notify_one();
  _datadog_thread.join();
#ifndef NDEBUG
  std::cout << "------------------------------------------------------------"
            << std::endl
            << "~Datadog" << std::endl
            << "------------------------------------------------------------"
            << std::endl;
#endif // NDEBUG
}

template <typename net_api>
void Datadog<net_api>::push(const MetricName name,
                            const unsigned long int value) {
  std::unique_lock<std::mutex> lck{_m};
  try {
    _metric_queue[name].emplace_back(value);
    ++_metric_queue_size;
    _logger.verbose("Metric queued: ", name, " => ", value);
  } catch (const std::exception &e) {
    _logger.error(logging::Error::metric_emplace, e.what());
  }
  lck.unlock();
  // _metric_queue_size might change after unlock, but doe not matter
  if (_metric_queue_size >= METRIC_MAX_QUEUE_ZISE)
    _cv.notify_one();
}
} // namespace DaaS::metrics
