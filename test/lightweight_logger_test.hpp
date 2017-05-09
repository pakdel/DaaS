#include <string>

#include "lightweight_logger.hpp"
#include "gtest/gtest.h"

// TEST(logging, LightweightLogger) {
TEST_F(LoggerTest, LightweightLogger) {
  // Capture cout in a buffer
  std::stringstream buffer;
  std::streambuf *old_cout = std::cout.rdbuf(buffer.rdbuf());
  // typedef LightweightLogger log_handler;

  auto logger = std::unique_ptr<LightweightLogger>(new LightweightLogger());
  logger->log("log message 1");
  logger->debug("log message 2");
  logger.reset();

  // Reset cout
  std::cout.rdbuf(old_cout);

  auto all_logs = buffer.str();
  auto position = all_logs.find("LOG     log message 1");
  ASSERT_FALSE(position == std::string::npos);
  position = all_logs.find("DEBUG   log message 2", position);
  ASSERT_FALSE(position == std::string::npos);

  position = all_logs.find("DEBUG   Starting process_logs_thread...");
  ASSERT_FALSE(position == std::string::npos);
  position = all_logs.find("DEBUG   Processed logs: 3", position);
  ASSERT_FALSE(position == std::string::npos);
}
