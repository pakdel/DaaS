#include "http_request_handler.hpp"

namespace DaaS {

void http_request_handler::pg_connect() {
  while (_running) { // && !pg->is_open());
    try {
      pg.reset(new pqxx::connection());
      break;
    } catch (const pqxx::broken_connection &e) {
      _logger.error(logging::Error::db_connect_failed, e.what());
    }
    std::this_thread::sleep_for(DB_CONNECTION_RETRY_SLEEP);
  }
  assert(!_running || pg->is_open());
  // FIXME
  // If not running, just return
  //  pg->prepare_now("test", "SELECT NOW();");
  pg->prepare("test", "SELECT NOW();");
  _logger.recover_if_has_error(logging::Error::db_connect_failed,
                               "Connected to the PostgreSQL database");
}

void http_request_handler::process_requests_thread() {
  // Make sure the error is set, it this thread continues first
  _logger.debug("Starting process_requests_thread...");

  // Not an error. merely not initialized.
  // Nevertheless, do not accept traffic.
  _logger.set_error(logging::Error::no_db_yet);
  pg_connect();
  _logger.unset_error(logging::Error::no_db_yet);

  unsigned long int request_queue_size = 0;
  while (_running || request_queue.size()) {
    request_queue_size = request_queue.size();
    // We have dequeued a request, and we do not go above the limit
    assert(request_queue_size < request_queue_limit);
    _logger.recover_if_has_error(logging::Error::request_queue_size,
                                 "request_queue size is below its limit: ",
                                 request_queue_size);

    auto request = request_queue.get();
    if (request == nullptr) {
      _logger.verbose("No HTTP request to precess!! Leaving?");
      continue;
    }

    //      auto remote_endpoint = request->req.remote_endpoint();
    //      std::cout << "Still in queue: " << request_queue_size
    //                << "\n\tCause of closure: "
    //                << nghttp2_http2_strerror(request->e) << " (" <<
    //                request->e
    //                << ")\n\tfrom " << remote_endpoint.address() << ":"
    //                << remote_endpoint.port() << std::endl;

    if (request->_error) {
      _logger.warn(logging::Warning::http_request_closed, " Cause of closure: ",
                   nghttp2_http2_strerror(request->_error), " (",
                   request->_error, ")");
      continue;
    }
    // It is going to be handled, so there is no need to call an on_close
    request->res.on_close(nullptr);

    // FIXME sleep for testing purposes only!
    // std::this_thread::sleep_for(550ms);

    // FIXME
    try {
      pqxx::nontransaction q(*pg);
      //      auto r = q.exec("SELECT NOW();");
      auto r = q.prepared("test").exec();
      assert(r.size() == 1);
      assert(r.capacity() == 1);
      std::ostringstream oss;
      oss << "Hello, world!\n";
      for (auto c : r) {
        oss << "NOW = " << c[0].as<std::string>() << '\n';
      }

      request->res.write_head(200);
      //      request->res.end("Hello, world!");
      request->res.end(oss.str());
    } catch (const pqxx::broken_connection &e) {
      // libpqxx "reactivation" will bring it back.
      // Let's just log an error and skip this one
      // FIXME need to clean this error
      _logger.error(logging::Error::db_connect_failed, "Failed to get ",
                    request->req.uri().path, " for ",
                    request->req.remote_endpoint().address(), " because: ",
                    e.what());
      request->res.cancel(NGHTTP2_INTERNAL_ERROR);
    }

// FIXME sleep for testing purposes only!
//    std::this_thread::sleep_for(950ms);

// TODO Maybe even in Release
#ifndef NDEBUG

    auto now = NOW();

    metrics_queue->push(metrics::MetricName::request_queue_size,
                        request_queue_size);
    metrics_queue->push(metrics::MetricName::server_overhead_time,
                        time_diff(request->_t_created, request->_t_queued));
    metrics_queue->push(metrics::MetricName::wait_time,
                        time_diff(request->_t_queued, request->_t_grabbed));
    metrics_queue->push(metrics::MetricName::process_time,
                        time_diff(request->_t_grabbed, now));
    metrics_queue->push(metrics::MetricName::total_response_time,
                        time_diff(request->_t_created, now));

    ++request_processed;
  }
  _logger.debug("process_requests_thread processed requests: ",
                request_processed, "\n\t\tremaining: ", request_queue.size());
#else
  }
#endif // NDEBUG
  pg->disconnect();
  // Maybe pg.reset();
}

void http_request_handler::push(const http_request &req,
                                const http_response &res) {
  if (request_queue.size() >= request_queue_limit) {
    _logger.error(logging::Error::request_queue_size,
                  "request_queue is at its limit: ", request_queue_limit);
    // res.write_head(503);
    // res.end("Too many requests in the queue. Try again later.");
    res.cancel(NGHTTP2_ENHANCE_YOUR_CALM);
    return;
  }
  // FIXME emplace instead
  std::unique_ptr<RequestData> req_data(new RequestData(req, res));
  request_queue.put(move(req_data));
}

http_request_handler::~http_request_handler() {
  _running = false;
  // Should not be necessary: Do not accept any new request
  // request_queue_limit = 0;
  request_queue.flush();
  requests_thread.join();
  assert(!pg->is_open());
#ifndef NDEBUG
  std::cout << "************************************************************"
            << std::endl
            << "~http_request_handler" << std::endl
            << "************************************************************"
            << std::endl;
#endif // NDEBUG
}

} // namespace DaaS
