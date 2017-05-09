#ifndef LOGGING_HPP
#define LOGGING_HPP

#include "logentries.hpp"
#include "logentries.tpp"

#include "net_api.hpp"

typedef DaaS::logging::LogEntries<NetAPI> log_handler;

#endif // LOGGING_HPP
