#pragma once

#include <vector>
#include <array>

class MRF;

namespace relaxed_rbp {
void solve(MRF* mrf, double sensitivity, uint64_t threads,
           std::vector<std::array<double,2> >* answer);

}
