// Copyright © 2021 Felix Schütz.
// Licensed under the MIT license. See the LICENSE file for details.

#include <random>

#include "resource.hpp"
#include "simcpp20.hpp"

struct config {
  int n_customers;
  double mean_arrival_interval;
  double max_wait_time;
  double mean_service_time;
  resource counters;
  std::default_random_engine gen;
  std::exponential_distribution<double> exp_dist;
};

simcpp20::event customer(simcpp20::simulation &sim, config &conf, int id) {
  printf("[%5.1f] Customer %d arrives\n", sim.now(), id);

  auto request = conf.counters.request();
  auto timeout = sim.timeout(conf.max_wait_time);
  // internal compiler error when directly awaiting return value of sim.any_of
  auto any_of = sim.any_of({request, timeout});
  co_await any_of;

  if (!request.triggered()) {
    request.abort();
    printf("[%5.1f] Customer %d leaves unhappy\n", sim.now(), id);
    co_return;
  }

  printf("[%5.1f] Customer %d gets to the counter\n", sim.now(), id);

  auto service_time = conf.exp_dist(conf.gen) * conf.mean_service_time;
  co_await sim.timeout(service_time);

  printf("[%5.1f] Customer %d leaves\n", sim.now(), id);
  conf.counters.release();
}

simcpp20::event customer_source(simcpp20::simulation &sim, config &conf) {
  for (int id = 1; id <= conf.n_customers; ++id) {
    auto proc = customer(sim, conf, id);

    auto arrival_interval =
        conf.exp_dist(conf.gen) * conf.mean_arrival_interval;
    co_await sim.timeout(arrival_interval);
  }
}

int main() {
  simcpp20::simulation sim;

  std::random_device rd;
  config conf{
      .n_customers = 10,
      .mean_arrival_interval = 10,
      .max_wait_time = 16,
      .mean_service_time = 12,
      .counters{sim, 1},
      .gen{std::default_random_engine{rd()}},
      .exp_dist{},
  };

  customer_source(sim, conf);

  sim.run();
}