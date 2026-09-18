#pragma once

#include <vector>
#include <array>
#include <chrono>
#include "perf_metrics.h"

class MRF;
class MRF_CSR;

namespace residual_bp_filtered_inserts_only {

void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2>>* answer, perf_metrics &metrics);

}

namespace residual_bp_filtered_inserts_only_CSR {

    void solve(MRF_CSR* mrf, double sensitivity,
        std::vector<std::array<double,2>>* answer, perf_metrics &metrics);
    
}
