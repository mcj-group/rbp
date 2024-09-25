#pragma once

#include <vector>
#include <array>

class MRF;

namespace heap_rbp {

void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer,
           int threadNum, int queueNum, int batchSizePop, int batchSizePush,
           int usePrefetch, int stickiness);

}