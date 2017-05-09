#include "shared_queue.hpp"
#include "gtest/gtest.h"

#include <string>

using namespace DaaS;

TEST(SharedQueues, QueuingWithoutMetrics) {
  shared_queue<std::string, false> test_queue;
  // These are all zero at the begining
  ASSERT_EQ(0, test_queue.size());
  auto load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);

  // Add first element
  test_queue.put(std::unique_ptr<std::string>(new std::string("Datum 1")));
  ASSERT_EQ(1, test_queue.size());
  load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);
  auto datum = test_queue.get();
  ASSERT_EQ(0, test_queue.size());
  load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);
  ASSERT_STREQ("Datum 1", datum->c_str());

  // Add 2nd and 3rd elements
  test_queue.put(std::unique_ptr<std::string>(new std::string("Datum 2")));
  test_queue.put(std::unique_ptr<std::string>(new std::string("Datum 3")));
  ASSERT_EQ(2, test_queue.size());
  load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);
  datum = test_queue.get();
  ASSERT_EQ(1, test_queue.size());
  load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);
  ASSERT_STREQ("Datum 2", datum->c_str());
  datum = test_queue.get();
  ASSERT_EQ(0, test_queue.size());
  load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);
  ASSERT_STREQ("Datum 3", datum->c_str());

  // If flush does not work properly, this will freeze!!!
  test_queue.flush();
  datum = test_queue.get();
  ASSERT_EQ(nullptr, datum);
}

TEST(SharedQueues, QueuingWithMetrics) {
  struct element {
    std::string msg;
    perf_timer _t_created, _t_queued, _t_grabbed /*, _t_done */;
    element(const char *msg) : msg(msg), _t_created(NOW()) {}
  };

  shared_queue<element, true> test_queue;
  // These are all zero at the begining
  ASSERT_EQ(0, test_queue.size());
  auto load = test_queue.load();
  ASSERT_EQ(0.0f, load[0]);
  ASSERT_EQ(0.0f, load[1]);

  // Add first element
  test_queue.put(std::unique_ptr<element>(new element("Datum 1")));
  ASSERT_EQ(1, test_queue.size());
  load = test_queue.load();
  ASSERT_GT(load[0], 0.0f);
  ASSERT_GT(load[1], 0.0f);
  auto previous_load = move(load);
  auto datum = test_queue.get();
  ASSERT_EQ(0, test_queue.size());
  load = test_queue.load();
  ASSERT_LT(load[0], previous_load[0]);
  ASSERT_LT(load[1], previous_load[1]);
  previous_load = move(load);
  ASSERT_STREQ("Datum 1", datum->msg.c_str());

  // Add 2nd and 3rd elements
  test_queue.put(std::unique_ptr<element>(new element("Datum 2")));
  test_queue.put(std::unique_ptr<element>(new element("Datum 3")));
  ASSERT_EQ(2, test_queue.size());
  load = test_queue.load();
  ASSERT_GT(load[0], previous_load[0]);
  ASSERT_GT(load[1], previous_load[1]);
  previous_load = move(load);
  datum = test_queue.get();
  ASSERT_EQ(1, test_queue.size());
  load = test_queue.load();
  ASSERT_LE(load[0], previous_load[0]);
  ASSERT_LE(load[1], previous_load[1]);
  previous_load = move(load);
  ASSERT_STREQ("Datum 2", datum->msg.c_str());
  datum = test_queue.get();
  ASSERT_EQ(0, test_queue.size());
  load = test_queue.load();
  ASSERT_LE(load[0], previous_load[0]);
  ASSERT_LE(load[1], previous_load[1]);
  previous_load = move(load);
  ASSERT_STREQ("Datum 3", datum->msg.c_str());

  // If flush does not work properly, this will freeze!!!
  test_queue.flush();
  datum = test_queue.get();
  ASSERT_EQ(nullptr, datum);
}
