#pragma once
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "mrf.h"
#include "perf_metrics.h"

//sequential (no multi-threading) versiosn of synchronous belief propagation algorithm

class MRF;
class MRF_CSR;
namespace synch_bp_sequential {
    void solve(MRF* mrf, double sensitivity, 
        std::vector<std::array<double,2>>* answer,
        perf_metrics& metrics);
}

namespace synch_bp_sequential_CSR {
    void solve(MRF_CSR* mrf, double sensitivity, 
        std::vector<std::array<double,2>>* answer,
        perf_metrics& metrics);
}
