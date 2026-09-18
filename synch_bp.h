#pragma once
#include <vector>
#include "perf_metrics.h"
class MRF;
class MRF_CSR;

//slightly different functions needed for LDPC, because using CSR, versus Ising/Potts/tree general MRF.
namespace synch_bp_CSR
{
    void solve(MRF_CSR *mrf, double sensitivity, uint64_t _threads,
               std::vector<std::array<double, 2>> *answer, perf_metrics &metrics,
               uint64_t timeout_seconds = 3600);
}

namespace synch_bp
{
    void solve(MRF *mrf, double sensitivity,
               std::vector<std::array<double, 2>> *answer, uint64_t _threads,
               perf_metrics &metrics, uint64_t timeout_seconds = 3600);
}
