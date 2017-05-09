// TODO implement batch queries, because we need to close to submit!!
// TODO Should have a file to dump errors into as a backup plan
// TODO recover, if had error
// TODO recover from warnings

/*
Notes:
- logger needs to be initialized before usage
- in order to register and error, just log it with error,
  and to clear an error, log it with recover
*/
// We need to include the source too, because it contains tempaltes :(
// It is at the bottom

#ifndef LOGENTRIES_HPP
#define LOGENTRIES_HPP

#include "DaaS_config.h"

#include "shared_queue.hpp"

#include <bitset>
#include <boost/asio.hpp>
#include <iomanip>
#include <iostream>
#include <thread>

namespace DaaS::logging {

enum Error {
  signal_received,
  request_queue_size,
  metric_emplace,
  log_emplace,
  logentries_net,
  datadog_net,
  no_db_yet,
  db_connect_failed,
  error_count,
};
constexpr unsigned int ErrorCount =
    static_cast<unsigned int>(Error::error_count);
static const char *ErrorString[ErrorCount]{
    "Received a Signal",
    "request_queue_size",
    "Failed to add to Log Queue",
    "Failed to add to Metric Queue",
    "Network error while trying to send data to Logentries",
    "Network error while trying to send data to Datadog",
    "Not connected to the database yet",
    "Failed to connect to the database"};

enum Warning {
  http_request_closed,
  datadog_wrong_size,
  datadog_invalid_response,
  warning_count
};
constexpr unsigned int WarningCount =
    static_cast<unsigned int>(Warning::warning_count);
static const char *WarningString[WarningCount]{"http_request closed with error",
                                               "Datadog: Wrong Response Size",
                                               "Datadog: Invalid Response"};

struct LogData {
public:
  boost::asio::streambuf msg;
  template <typename... Args>
  LogData(const char *debug_level, std::string token, Args... args);
  LogData() = delete;
  // LogData(string msg): msg(msg) {}
  // LogData(string arg) {
  //     ostream msg_stream(&msg);
  //     msg_stream << "Not initialized!!!";
  // }
  LogData(std::string msg) = delete;
};

template <typename net_api> class LogEntries {
private:
  std::unique_ptr<net_api> _net_api;
  std::string _token;

  std::queue<std::unique_ptr<LogData>> _log_queue;
  std::mutex _m;
  std::condition_variable _cv;

  std::atomic_bool _running;
  std::thread _logger_thread;
#ifndef NDEBUG
  unsigned int logs_processed{0};
#endif // NDEBUG
  void process_logs_thread();

  std::bitset<ErrorCount> error_bitset = 0;
  std::bitset<WarningCount> warning_bitset = 0;
  LogEntries() : _running(false) {}

  template <typename... Args>
  void enqueue(const char *debug_level, std::string token, Args... logs);

public:
  LogEntries(LogEntries const &) = delete;
  LogEntries(LogEntries &) = delete;  // non construction-copyable
  LogEntries(LogEntries &&) = delete; // non construction-movable
  LogEntries &operator=(const LogEntries &) = delete; // non copy assignable
  LogEntries &operator=(LogEntries &&) = delete;      // non move assignable
  ~LogEntries();
#ifdef TEST
  inline void destroy() {
    _running = false;
    _cv.notify_one();
    _logger_thread.join();
    _logger_thread = std::thread([&] {
      std::unique_lock<std::mutex> lck{_m};
      _cv.wait(lck);
    });
  }
  inline void flush() {
    while (_running && !_log_queue.empty()) {
      if (!_log_queue.empty())
        _cv.notify_one();
      std::this_thread::sleep_for(100ms);
    }
  }
#endif

  static LogEntries<net_api> &getInstance() {
    static LogEntries<net_api> instance;
    return instance;
  }
  void initialize(std::string host, std::string port, std::string token) {
    _net_api = std::unique_ptr<net_api>(new net_api(host, port));
    _token = token;
    _running = true;
    _logger_thread = std::thread(&LogEntries::process_logs_thread, this);
  }

  inline void set_error(const Error error) {
    error_bitset.set(static_cast<size_t>(error), true);
  }
  inline void unset_error(const Error error) {
    error_bitset.set(static_cast<size_t>(error), false);
  }
  inline bool has_error(const Error error) {
    return error_bitset[static_cast<unsigned int>(error)];
  }
  inline void set_warning(const Warning warning) {
    warning_bitset.set(static_cast<size_t>(warning), true);
  }
  inline void unset_warning(const Warning warning) {
    warning_bitset.set(static_cast<size_t>(warning), false);
  }
  inline bool has_warning(const Warning warning) {
    return warning_bitset[static_cast<unsigned int>(warning)];
  }
  inline bool any_error() { return error_bitset.any(); }
  inline bool any_warning() { return warning_bitset.any(); }
  //  inline bool healthy() { return error_bitset.none(); }
  std::string error_string();
  std::string warning_string();

  template <typename... Args>
  inline void recover(const Error error, Args... logs) {
    unset_error(error);
    enqueue("RECOVERY", _token, logs...);
  }
  template <typename... Args>
  inline void recover_if_has_error(const Error error, Args... logs) {
    if (!has_error(error))
      return;
    unset_error(error);
    enqueue("RECOVERY", _token, logs...);
  }
  template <typename... Args>
  inline void recover(const Warning warning, Args... logs) {
    unset_warning(warning);
    enqueue("RECOVERY", _token, logs...);
  }

  template <typename... Args> inline void disaster(Args... logs) {
    std::cerr << "A disaster happened: ";
    (std::cerr << ... << logs) << "\nExiting...." << std::endl;
    enqueue("DISASTER", _token, logs...);
    raise(SIGTERM);
  }

  template <typename... Args>
  inline void error(const Error error, Args... logs) {
    set_error(error);
    enqueue("ERROR", _token, ErrorString[static_cast<size_t>(error)], ' ',
            logs...);
  }
  template <typename... Args>
  inline void warn(const Warning warning, Args... logs) {
    set_warning(warning);
    enqueue("WARNING", _token, WarningString[static_cast<size_t>(warning)], ' ',
            logs...);
  }
  template <typename... Args> inline void log(Args... logs) {
    enqueue("LOG", _token, logs...);
  }
  template <typename... Args> inline void debug(Args... logs) {
    if (DEBUG_LEVEL > 0)
      enqueue("DEBUG", _token, logs...);
  }
  template <typename... Args> inline void trace(Args... logs) {
    if (DEBUG_LEVEL > 1)
      enqueue("TRACE", _token, logs...);
  }
  template <typename... Args> inline void verbose(Args... logs) {
    if (DEBUG_LEVEL > 2)
      enqueue("VERBOSE", _token, logs...);
  }
};

} // namespace DaaS::logging

#endif // LOGENTRIES_HPP
