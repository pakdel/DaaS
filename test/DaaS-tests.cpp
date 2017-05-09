#include "gtest/gtest.h"
#include <DaaS_config.h>

#include "net_api_mock.hpp"
#include "redirect_stdout.hpp"

#include "logging_test.hpp"
#include "metrics_test.hpp"
#include "shared_queue_test.hpp"

#include "http_request_handler_test.hpp"

int main(int argc, char **argv) try {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} catch (const std::exception &e) {
  std::cerr << "Got an exception: " << e.what() << std::endl;
  return (1);
}
