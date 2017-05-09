// FIXME diabled load calculation
// FIXME
// http://blogs.msmvps.com/vandooren/2007/01/05/creating-a-thread-safe-producer-consumer-queue-in-c-without-using-locks/
// TODO Try a roundrobin queue / CircularQueue / Ring Bufer:
// http://www.programming-techniques.com/2011/11/in-linear-queue-when-we-delete-any.html
// http://www.whatprogramming.com/cplusplus/circular-queue/
// http://electrofriends.com/source-codes/software-programs/cpp-programs/cpp-advanced-programs/c-program-to-implement-circular-queue-using-array/
//
//
// TODO https://theboostcpplibraries.com/boost.lockfree
// TODO Try w/o move()
// TODO We might need to do more in the 'flush', instead of /* simply leave */
// TODO Try std::enable_if for partial specialization
// http://en.cppreference.com/w/cpp/types/enable_if
// static_assert(dependent_false<T...>::value, "you are passing the wrong
// arguments!");
// getvalue<T>::value
// std::is_same_v<T,T>
// Partial specialization
// http://stackoverflow.com/questions/23970532/what-do-compilers-do-with-compile-time-branching
// #include <type_traits>

// We need to include the source too, because it contains tempaltes :(
// It is at the bottom

// Cannot depend on the logging facility,
// because it relies on the queuing!
// #include "logging.hpp"

#ifndef SHARED_QUEUE_HPP
#define SHARED_QUEUE_HPP

#include "DaaS_config.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <math.h>
#include <queue>

namespace DaaS {

template <typename T, const bool metrics> class shared_queue {

private:
  typedef std::unique_ptr<T> element;
  std::queue<element> _q;
  std::mutex _m;
  std::condition_variable _cv;
  // Queue limit is handled bye the http_request_handler
  // because that is the only one that uses this limit
  // const rlim_t _l;
  std::atomic_bool _running;
  perf_timer _t;
  /* Load averages as fixed-point, 1min and 5min */
  unsigned long aveq[2] = {0ul, 0ul};
  // long double float_load_averages[2] = {0.0, 0.0};
  // inline maybe?
  void calc_load();

public:
  shared_queue() : _running(true) {}
  //    inline auto __attribute__ ((deprecated)) count() const { return
  //    _count.load(); }
  //    constexpr int __attribute__ ((deprecated)) count() const { return 0; };
  inline auto load() const;
  inline auto size() const { return _q.size(); }
  inline void flush() {
    /* simply leave */
    _running = false;
    _cv.notify_one();
  }
  //  inline void lock() { _m.lock(); }
  void put(element datum);
  element get();
}; // shared_queue

} // namespace DaaS

#include "shared_queue.tpp"

#endif // SHARED_QUEUE_HPP
