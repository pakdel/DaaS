// FIXME Recover from warnings
// TODO https://github.com/tvaneerd/cpp17_in_TTs/blob/master/ALL_IN_ONE.md
// std::filesystem
// TODO static_assert(dependent_false<T>::value, "Must be arithmetic");

/*
Notes
=====
- SIGUSR2: Restart the same binary, even if it is deleted
- When reaches Open Files' limit: `terminate called without an active exception`
```
cat /proc/sys/net/ipv4/tcp_max_syn_backlog
echo 1 > /proc/sys/net/ipv4/tcp_abort_on_overflow
```
- std::cerr is used in cae of disaster

Globals
=======
- http_server and restart_signal: because of signal handling
- global_error is a static member of DaaS::logging
- log_handler type is defined in logging.hpp
*/
#include "DaaS_config.h"

#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "\tread " LOGENTRIES_TOKEN_ENV_VAR "      # Logentries Token\n"              \
  "\tread " DATADOG_API_KEY_ENV_VAR "       # Datadog API Key\n"               \
  "\texport " LOGENTRIES_TOKEN_ENV_VAR " " DATADOG_API_KEY_ENV_VAR             \
  "\t./DaaS\n"                                                                 \
  "PostgreSQL Environment Variables: "                                         \
  "https://www.postgresql.org/docs/current/static/libpq-envars.html"           \
  "PostgreSQL Credentials: "                                                   \
  "https://www.postgresql.org/docs/current/static/libpq-pgpass.html"           \
  "We also need server.key and server.crt"

#ifdef _WIN32
#error Needs an OS!
#endif

#include <condition_variable>
#include <csignal>
#include <string>
#include <unistd.h>

using namespace DaaS;

#include "logging.hpp"

#include "metrics.hpp"

#include "http_request_handler.hpp"

static std::unique_ptr<nghttp2_server> http_server;

/* Disabling the inotify_monitor for now
#include "inotify_monitor.hpp"
*/

// No reason to exit, yet
static auto restart_signal = 0;
#define EXIT(x, y)                                                             \
  std::cerr << std::setw(5) << std::left << __LINE__ << x << std::endl;        \
  restart_signal = 0;                                                          \
  return (y)

inline void signal_handler(int signal_) {
  // Since we stop the http_server immediately after, the new state cannot be
  // queried via /health
  log_handler::getInstance().error(logging::Error::signal_received,
                                   "Interrupt signal ", signal_, " received.");
  //  std::cout << "Interrupt signal " << signal_ << " received." << std::endl;
  restart_signal = signal_;
  http_server->stop();

  /* Disabling the inotify_monitor for now
  // Notify the iNotify
  auto b = "Got a Signal";
  int s = write(event_fd, b, sizeof(b));
  logger.debug("Wrote to event_fd: ", s, " (errno: ", errno,
  ")");
  */
}

inline std::string get_my_name() {
  char buf[MAX_FILEPATH_SIZE];
  auto len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len == -1) {
    std::cout << "readlink failed: " << errno << std::endl;
    return std::string();
  }
  buf[len] = '\0';
  return std::string(buf);
}

int main() try {
  auto LE_TOKEN = getenv(LOGENTRIES_TOKEN_ENV_VAR);
  auto DD_API_KEY = getenv(DATADOG_API_KEY_ENV_VAR);
  if (DD_API_KEY == nullptr || LE_TOKEN == nullptr) {
    EXIT(USAGE, EXIT_BAD_INVOKATION);
  }
  std::string my_name = get_my_name();

/*
// no need to hide command line arguments
char *arg_end = argv[1] + strlen (argv[1]);
*arg_end = ' ';
*/
#if DEBUG_LEVEL > 0
  std::cout << "Debug mode. No daemonize!\n\tVersion " << PROJECT_VERSION
            << std::endl;
#else
  fclose(stdin);
  fclose(stdout);
  // std::cerr is used in cae of disaster
  // fclose(stderr);
  if (daemon(0, -1)) {
    EXIT("Failed to daemonize!", EXIT_DAEMONIZE_FAILED);
  }
#endif

  /* Disabling the inotify_monitor for now
  // We already did the 'daemon' (Fork / Exec), so we can use *_CLOEXEC
  // This is the file descriptor for notifying inotify watch
  // We don't want to wait forever for a change, if got a signal
  int event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (event_fd < 0) { // efd == -1
      EXIT("eventfd failed: " << errno, EXIT_EVENTFD_FAILED);
  }
  thread inotify_monitor_thread {inotify_monitor, my_name.c_str(), event_fd};
  */

  // First thing to do is setting up the logger
  // And then, in this order: metrics_queue, request_queue and http_server
  log_handler &logger = log_handler::getInstance();
  logger.initialize(LOGENTRIES_HOST, LOGENTRIES_PORT, LE_TOKEN);

  // Adding metrics
  // Only the request_queue is using it
  auto metrics_queue =
      std::make_shared<metric_handler>(DATADOG_HOST, DATADOG_PORT, DD_API_KEY);

  // Sets the maximum length of the queue of http_request_handler
  struct rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
    EXIT("FAILED to get RLIMIT_NOFILE", EXIT_RLIMIT_FAILED);
  }
  logger.log("Open File Soft Limit: ", limit.rlim_cur);
  logger.log("http_request_handler Queue Size Limit: ",
             (limit.rlim_cur - OPEN_FILE_MARGIN));
  auto request_queue =
      std::unique_ptr<http_request_handler>(new http_request_handler(
          (limit.rlim_cur - OPEN_FILE_MARGIN), metrics_queue));

  boost::system::error_code ec;
  boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
  tls.use_private_key_file(PRIVATE_KEY_FILE, boost::asio::ssl::context::pem);
  tls.use_certificate_chain_file(CERTIFICATE_CHAIN_FILE);
  nghttp2::asio_http2::server::configure_tls_context_easy(ec, tls);

  http_server = std::unique_ptr<nghttp2_server>(new nghttp2_server);
  http_server->handle(
      "/", [&](const http_request &req, const http_response &res) {
        auto remote_endpoint = req.remote_endpoint();
        logger.verbose("Got a request for '", req.uri().path, "' from ",
                       remote_endpoint.address(), ":", remote_endpoint.port());
        request_queue->push(req, res);
        logger.verbose("Request for '", req.uri().path, "' pushed to queue");
      });
  // This has to be extremely fast
  http_server->handle("/health", [&](const http_request &req,
                                     const http_response &res) {
    auto remote_endpoint = req.remote_endpoint();
    logger.verbose("Got a request for '/health' from ",
                   remote_endpoint.address(), ":", remote_endpoint.port());
    if (logger.any_error()) {
      res.write_head(500);
      std::ostringstream errors;
      errors << "{\"status\":\"ERROR\",\"errors\":[" << logger.error_string()
             << "], \"warnings\":[" << logger.warning_string() << "]}";
      res.end(errors.str());
      return;
    }
    //    else
    if (logger.any_warning()) {
      res.write_head(200);
      std::ostringstream errors;
      errors << "{\"status\":\"WARNING\",\"errors\":[], \"warnings\":["
             << logger.warning_string() << "]}";
      res.end(errors.str());
      return;
    }
    //    else {
    res.write_head(200);
    res.end("{\"status\": \"OK\",\"errors\": []}");
    //    }
  });

  // Sets number of native threads to handle incoming HTTP request.
  // It defaults to 1.
  // 2 shold be enough to keep the queue full
  http_server->num_threads(THREAD_COUNT);
  // Sets the maximum length to which the queue of pending
  // connections.
  // http_server->backlog(4096);

  // setup clean shutdown
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGUSR1, signal_handler);
  signal(SIGUSR2, signal_handler);

  if (http_server->listen_and_serve(ec, tls, "0.0.0.0", PORT)) {
    std::cerr << "error: " << ec.message() << std::endl;
  }

  if (restart_signal == SIGUSR1 || restart_signal == SIGUSR2) {
    if (restart_signal == SIGUSR2)
      my_name =
          "/proc/self/exe"; // Executes the same binary, even if it is deleted
    logger.trace("My name is ", get_my_name());
    logger.debug("Going to execl ", my_name);

    // Instead of fork, we can explicitly release in reverse order of
    // initialization: http_server, request_queue and metrics_queue
    // Then continue on with an exec
    //    // Not necessary: http_server->stop() / http_server->join()
    //    http_server.reset();
    //    logger.log("Server should be down now.");
    //    // No more request handling
    //    request_queue.reset();
    //    logger.log("Request Queue should be down now.");
    //    // No more metrics
    //    metrics_queue.reset();
    //    logger.log("Metrics Queue should be down now.");
    //    // No more logging
    //    logger.reset();
    // TODO Security: Potential insecure implementation-specific behavior in
    // call 'vfork': Call to function 'vfork' is insecure as it can lead to
    // denial of service situations in the parent process. Replace calls to
    // vfork with calls to the safer 'posix_spawn' function
    pid_t pid = vfork();
    if (pid == -1) {
      EXIT("Failed to fork a restart process: " << errno,
           EXIT_RESTART_FORK_FAILED);
    } else if (pid == 0) {
      auto err =
          execl(my_name.c_str(), "DaaS_restarted", static_cast<char *>(NULL));

      EXIT("Failed to restart: " << errno << "(" << err << " == -1)",
           EXIT_RESTART_EXEC_FAILED);
    }

    // if (pid > 0) continue to exit
  }

  return 0;
} catch (const std::exception &e) {
  EXIT(e.what(), EXIT_EXCEPTION);
}
