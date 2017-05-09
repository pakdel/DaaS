#include "gtest/gtest.h"

#include "net_api_mock.hpp"
#include "redirect_stdout.hpp"

#include "lightweight_logger.hpp"
#include "logentries.hpp"
#include "logentries.tpp"

// Prevent redefinition of log_handler
#define LOGGING_HPP

typedef DaaS::logging::LogEntries<net_api_mock> log_handler;
using namespace DaaS;

TEST_F(RedirectSTDOUT, LogEntries) {
  log_handler &logger = log_handler::getInstance();
  logger.initialize("localhost.localdomain", "http",
                    "00000000-OOOO-0000-OOOO-000000000000");
  logger.log("log message 1");
  logger.debug("log message 2");
  // It is used in the rest of the tests, so we cannot destruct it
  //  logger.~LogEntries();
  //  logger.destroy();
  logger.flush();
#ifndef NDEBUG
  std::cerr
      << "\nThe following will be printed out during destruction of the logger "
         "upon exit:\n"
         "send 00000000-OOOO-0000-OOOO-000000000000 LOG Processed logs: 4\n"
         "============================================================\n"
         "~LogEntries\n"
         "============================================================\n"
      << std::endl;
#endif // NDEBUG

  auto all_logs = buffer.str();
  auto position = all_logs.find("endpoint: localhost.localdomain:http\n");
  ASSERT_TRUE(position == 0);

#ifndef NDEBUG
  position = all_logs.find("send 00000000-OOOO-0000-OOOO-000000000000 "
                           "DEBUG Starting process_logs_thread...\n");
  ASSERT_FALSE(position == std::string::npos);
#endif // NDEBUG

  position = all_logs.find("send 00000000-OOOO-0000-OOOO-000000000000 "
                           "LOG log message 1\n");
  ASSERT_FALSE(position == std::string::npos);

#ifndef NDEBUG
  position = all_logs.find("send 00000000-OOOO-0000-OOOO-000000000000 "
                           "DEBUG log message 2\n",
                           position);
  ASSERT_FALSE(position == std::string::npos);
#endif // NDEBUG

  //  ASSERT_STREQ(
  //      "endpoint: localhost.localdomain:http\n"
  //      "send 00000000-OOOO-0000-OOOO-000000000000 "
  //      "DEBUG Starting process_logs_thread...\n"
  //      "send 00000000-OOOO-0000-OOOO-000000000000 "
  //      "LOG log message\n"
  //      "send 00000000-OOOO-0000-OOOO-000000000000 "
  //      "LOG Processed logs: 2\n"
  //      // "============================================================\n"
  //      //               "~LogEntries\n"
  //      // "============================================================\n"
  //      ,
  //      buffer.str().c_str());
}

TEST_F(RedirectSTDOUT, LightweightLogger) {
  auto logger = std::unique_ptr<LightweightLogger>(new LightweightLogger());
  logger->log("log message 1");
  logger->debug("log message 2");
  logger.reset();

  auto all_logs = buffer.str();
  auto position = all_logs.find("LOG     log message 1");
  ASSERT_FALSE(position == std::string::npos);
#ifndef NDEBUG
  position = all_logs.find("DEBUG   log message 2", position);
  ASSERT_FALSE(position == std::string::npos);

  position = all_logs.find("DEBUG   Starting process_logs_thread...");
  ASSERT_FALSE(position == std::string::npos);
#endif // NDEBUG

  //  ASSERT_STREQ("DEBUG   Starting process_logs_thread...\n"
  //               "LOG     log message 1\n"
  //               "DEBUG   log message 2\n"
  //               "============================================================\n"
  //               "~LightweightLogger\n"
  //               "============================================================\n",
  //               buffer.str().c_str());
}
