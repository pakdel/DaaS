#ifndef METRICS_HPP
#define METRICS_HPP

#include "datadog.hpp"
#include "datadog.tpp"

#include "net_api.hpp"

typedef DaaS::metrics::Datadog<NetAPI> metric_handler;

#endif // METRICS_HPP
