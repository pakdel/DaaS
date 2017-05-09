/* Since this is a class template, the Template file is included at the bottom
 * #include "http_request_handler.tpp"
 */
// FIXME Do not freeze if cannot connect
// TODO Implement soft limit for the queue size
// If promise / future is used, the http_server thread will be blocked
#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

#include "DaaS_config.h"

// These are included in a higher level to accomodate for UnitTesting
// #include "logging.hpp"
// #include "metrics.hpp"

#include <iostream>
#include <pqxx/pqxx> // http://pqxx.org/devprojects/libpqxx/doc/4.0/html/Reference/a00023.html
#include <sys/resource.h>
#include <thread>

namespace DaaS {

class http_request_handler {
private:
  class RequestData {
  public:
    const http_request &req;
    const http_response &res;
    uint32_t _error;
#ifndef NDEBUG
    perf_timer _t_created, _t_queued, _t_grabbed /*, _t_done */;
#endif // NDEBUG

    inline explicit RequestData(const http_request &req,
                                const http_response &res)
        : req(req), res(res), _error(NGHTTP2_NO_ERROR)
#ifndef NDEBUG
          ,
          _t_created(NOW())
#endif // NDEBUG
    {
      res.on_close([&](uint32_t error_code) {
        // If it is already handles w/o error,
        // this callback should be already removed
        assert(error_code);
        // Mybe it iould be better to completely remove it from the queue
        _error = error_code;
        // It is handled in process_requests_thread
        //        log_handler::getInstance().debug(
        //            "Connection closed. Cause of closure: ",
        //            nghttp2_http2_strerror(error_code), " (", error_code, ")"
        //            // , " / Status Coode: ", res.status_code()
        //            );
      });
    }
  };

  // It needs to ne initialized early, because it is used everywhere
  log_handler &_logger;

  std::atomic_bool _running;
  rlim_t request_queue_limit;
  std::shared_ptr<metric_handler> metrics_queue;
#ifndef NDEBUG
  unsigned int request_processed{0};
  shared_queue<RequestData, true> request_queue;
#else  // NDEBUG
  shared_queue<RequestData, false> request_queue;
#endif // NDEBUG

  std::unique_ptr<pqxx::connection> pg;
  void pg_connect();

  std::thread requests_thread;
  void process_requests_thread();

public:
  inline explicit http_request_handler(
      rlim_t l, std::shared_ptr<metric_handler> metrics_queue)
      : _logger(log_handler::getInstance()), _running(true),
        request_queue_limit(l), metrics_queue(metrics_queue),
        requests_thread(&http_request_handler::process_requests_thread, this) {
    // Make sure the error is set, it this thread continues first
    _logger.set_error(logging::Error::no_db_yet);
  }
  void push(const http_request &req, const http_response &res);
  ~http_request_handler();
};

} // namespace DaaS

#include "http_request_handler.tpp"

#endif // HTTP_REQUEST_HANDLER_HPP
