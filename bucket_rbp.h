#pragma once

#include <vector>
#include <array>

class MRF;

namespace bucket_rbp {

void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer,
           int threadNum, int queueNum, int batchSizePop, int batchSizePush,
           int delta, int bucketNum, int usePrefetch, int stickiness);

}