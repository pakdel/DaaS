#ifndef REDIRECT_STDOUT_HPP
#define REDIRECT_STDOUT_HPP

#include "gtest/gtest.h"

#include <iostream>
#include <string>

// The fixture for testing Logger
class RedirectSTDOUT : public ::testing::Test {
protected:
  // Capture cout in a buffer
  std::stringstream buffer;
  //  std::streambuf *old_cout = std::cout.rdbuf(buffer.rdbuf());
  std::streambuf *old_cout;

protected:
  // You can do set-up work for each test here.
  RedirectSTDOUT() { old_cout = std::cout.rdbuf(buffer.rdbuf()); }

  // You can do clean-up work that doesn't throw exceptions here.
  virtual ~RedirectSTDOUT() { std::cout.rdbuf(old_cout); }

  //  virtual void SetUp() { old_cout = std::cout.rdbuf(buffer.rdbuf()); }

  // Reset cout
  //  virtual void TearDown() {
  //    std::cout.rdbuf(old_cout);
  //    std::cout << buffer.str();
  //  }
};

#endif // REDIRECT_STDOUT_HPP
