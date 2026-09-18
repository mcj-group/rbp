#pragma once

#include <vector>
#include <array>
#include "perf_metrics.h"

class MRF;

namespace relaxed_smart_splash {
void solve_mq(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2>>* answer,
           uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
           int splashH, 
           perf_metrics &metrics);

void solve_smq(MRF* mrf, double sensitivity,
            std::vector<std::array<double,2>>* answer,
            uint64_t threads,
            int splashH, perf_metrics &metrics);
}

namespace relaxed_smart_splash_CSR {
void solve_mq(MRF_CSR* mrf, double sensitivity,
            std::vector<std::array<double,2>>* answer,
            uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
            int splashH, 
            perf_metrics &metrics);

void solve_smq(MRF_CSR* mrf, double sensitivity,
    std::vector<std::array<double,2>>* answer,
    uint64_t threads,
    int splashH, perf_metrics &metrics);
}
