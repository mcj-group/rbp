#pragma once

#include <vector>
#include <array>

class MRF;

namespace residual_bp {

void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer);

} // namespace residual_bp
