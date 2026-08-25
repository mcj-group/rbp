#pragma once

#include <vector>
#include <array>
#include "perf_metrics.h"

class MRF;
class MRF_CSR;

namespace relaxed_rbp {

void solve_mq(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer,
           uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
           perf_metrics &metrics);

void solve_smq(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer,
           uint64_t threads,
           perf_metrics &metrics);
}

namespace relaxed_rbp_CSR {
    void solve_mq(MRF_CSR* mrf, double sensitivity,
            std::vector<std::array<double,2>>* answer,
            uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
            perf_metrics &metrics);
    
    void solve_smq(MRF_CSR* mrf, double sensitivity,
            std::vector<std::array<double,2>>* answer,
            uint64_t threads,
            perf_metrics &metrics);
}
