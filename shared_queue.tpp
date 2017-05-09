#include "shared_queue.hpp"

// Copied from Linux kernel
// http://www.linuxjournal.com/article/9001?page=0,1
// https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
// #define FSHIFT   11        /* nr of bits of precision */
#define FSHIFT 14 /* nr of bits of precision */
// #define FSHIFT   16        /* nr of bits of precision */
#define FIXED_1 (1 << FSHIFT) /* 1.0 as fixed-point */
// #define FIXED_LOG 7.6246189861593985L /* log(1<<11) */
#define FIXED_LOG 9.704060527839234L /* log(1<<14) */
// #define FIXED_LOG 11.090354888959125L /* log(1<<16) */
// exp_ = (FIXED_1) / exp(delta_t/Window Seconds)
// #define EXP_1  2014      /* 1/exp(1sec/5min) as fixed-point */
#define CALC_LOAD(load, exp, n)                                                \
  load *= exp;                                                                 \
  load >>= FSHIFT;                                                             \
  load += n * (FIXED_1 - exp);

namespace DaaS {

template <typename T, const bool metrics>
void shared_queue<T, metrics>::calc_load() {
  perf_timer _now = NOW();
  auto dt =
      std::chrono::duration_cast<std::chrono::microseconds>(_now - _t).count();
  // FIXME calcualte load less frequently
  // To prevent too much extra load, in case of a huge load (what?)
  // return if dt < 1 second
  static_assert(std::is_same<decltype(dt), signed long>::value,
                "dt must be a signed long");
  _t = _now;
  unsigned long e1 = exp(static_cast<long double>(dt) / -60000000 + FIXED_LOG);
  unsigned long e5 = exp(static_cast<long double>(dt) / -300000000 + FIXED_LOG);
  auto n = size();
  static_assert(
      std::is_same<decltype(n), typename std::queue<T>::size_type>::value,
      "n must be an int");
  CALC_LOAD(aveq[0], e1, n);
  CALC_LOAD(aveq[1], e5, n);

  // auto float_e1 = exp((long double)dt/ -60000000);
  // auto float_e5 = exp((long double)dt/-300000000);
  // static_assert(is_same<decltype(float_e1), long double>::value, "float_e1
  // must be a long double");
  // static_assert(is_same<decltype(float_e5), long double>::value, "float_e1
  // must be a long double");
  // float_load_averages[0] *= float_e1;
  // float_load_averages[0] += n*(1.0-float_e1);
  // float_load_averages[1] *= float_e5;
  // float_load_averages[1] += n*(1.0-float_e5);
}

template <typename T, const bool metrics>
auto shared_queue<T, metrics>::load() const {
  std::array<float, 2> load = {
      {static_cast<float>(aveq[0]) / static_cast<float> FIXED_1,
       static_cast<float>(aveq[1]) / static_cast<float> FIXED_1}};
  return load;
}

template <typename T, const bool metrics>
void shared_queue<T, metrics>::put(element datum) {
  if
    constexpr(metrics) datum->_t_queued = NOW();
  std::unique_lock<std::mutex> lck{_m};
  // if (_l && _q.size() >= _l ) {
  //     // Cannot depend on the logging facility,
  //     // because it relies on the queuing!
  //     // logger->error(e.what());
  //     // cout << e.what();
  //     cerr << "Reached maximum queue size: " << _l << endl;
  //     return;
  // }
  try {
    _q.push(move(datum));
  } catch (const std::exception &e) {
    // Cannot depend on the logging facility,
    // because it relies on the queuing!
    // logger->error(e.what());
    // cout << e.what();
    std::cerr << e.what();
    // exit(EXIT_EXCEPTION);
  }
  lck.unlock();
  _cv.notify_one();
  if
    constexpr(metrics and DEBUG_LEVEL > 2) calc_load();
}

template <typename T, const bool metrics>
typename shared_queue<T, metrics>::element shared_queue<T, metrics>::get() {
  std::unique_lock<std::mutex> lck{_m};
  // _cv.wait(lck);
  // if (_q.empty()) _cv.wait(lck, [=]{ return !(_q.empty() && _running)); });
  while (_q.empty() && _running)
    _cv.wait(lck);
  // Mutex is aquired by _cv.wait
  // Keep returning results if there is any
  // if (!_running) return nullptr;
  if (_q.empty())
    return nullptr;
  auto datum = move(_q.front());
  _q.pop();

  if
    constexpr(metrics) datum->_t_grabbed = NOW();
  if
    constexpr(metrics and DEBUG_LEVEL > 2) calc_load();
  return datum;
}

} // namespace DaaS
