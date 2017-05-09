#include "logentries.hpp"

namespace DaaS::logging {

template <typename... Args>
LogData::LogData(const char *debug_level, std::string token, Args... args) {
  std::ostream msg_stream(&msg);
  msg_stream << token << ' ' << debug_level << ' ';
  auto dummy = (msg_stream << ... << args) && 0;
  static_cast<void>(dummy);
  msg_stream << '\n';
}

// TODO Not a fan of this copy/paste
template <typename net_api> std::string LogEntries<net_api>::warning_string() {
  std::ostringstream oss;
  char separator = ' ';
  for (unsigned int i = 0; i < WarningCount; ++i) {
    if (warning_bitset[i]) {
      oss << separator << '"' << WarningString[i] << '"';
      separator = ',';
    }
  }
  return oss.str();
}

template <typename net_api> std::string LogEntries<net_api>::error_string() {
  std::ostringstream oss;
  char separator = ' ';
  for (unsigned int i = 0; i < ErrorCount; ++i) {
    if (error_bitset[i]) {
      oss << separator << '"' << ErrorString[i] << '"';
      separator = ',';
    }
  }
  return oss.str();
}

template <typename net_api>
template <typename... Args>
void LogEntries<net_api>::enqueue(const char *debug_level, std::string token,
                                  Args... logs) {
  std::unique_lock<std::mutex> lck{_m};
  try {
    //    _log_queue.push(
    //        std::unique_ptr<LogData>(new LogData(debug_level, token,
    //        logs...)));
    _log_queue.emplace(new LogData(debug_level, token, logs...));
    error_bitset.set(static_cast<size_t>(Error::log_emplace), false);
  } catch (const std::exception &e) {
    error_bitset.set(static_cast<size_t>(Error::log_emplace));
    std::cerr << e.what();
    (std::cerr << ... << logs) << std::endl;
  }
  lck.unlock();
  _cv.notify_one();
}

template <typename net_api> void LogEntries<net_api>::process_logs_thread() {
  debug("Starting process_logs_thread...");

  while (_running || !_log_queue.empty()) {
    std::unique_lock<std::mutex> lck{_m};
    while (_running && _log_queue.empty())
      _cv.wait(lck);
    // Mutex is aquired by _cv.wait

    if (/* !_running && */ _log_queue.empty()) {
      assert(!_running);
      // Might not be needed!
      lck.unlock();
      break;
    }

    auto tmp_log = std::move(_log_queue.front());
    _log_queue.pop();
    lck.unlock();

    if (!_net_api->send(tmp_log->msg.data()))
      error_bitset.set(static_cast<size_t>(Error::logentries_net));
    else
      error_bitset.set(static_cast<size_t>(Error::logentries_net), false);

// There is nothing to be read, but keep the socket open
//    _net_api->close();
#ifndef NDEBUG
    ++logs_processed;
  }
  // We are out of the log handling loop
  boost::asio::streambuf msg;
  std::ostream msg_stream(&msg);
  msg_stream << _token << " LOG Processed logs: " << logs_processed << "\n";
  _net_api->send(msg.data());
#else
  }
#endif // NDEBUG
}

template <typename net_api> LogEntries<net_api>::~LogEntries() {
  _running = false;
  _cv.notify_one();
  _logger_thread.join();
#ifndef NDEBUG
  std::cout << "============================================================"
            << std::endl
            << "~LogEntries" << std::endl
            << "============================================================"
            << std::endl;
#endif // NDEBUG
}

} // namespace DaaS::logging
