#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace utils {

// TODO(mcj) define a global LENGTH = 2
static inline double logSum(const std::array<double,2> logs) {
    constexpr double NINF = -std::numeric_limits<double>::infinity();
    double maxLog = NINF;
    for (double log : logs) {
        maxLog = std::max(maxLog, log);
    }
    if (maxLog == NINF) return NINF;

    double sumExp = 0.0;
    for (double log : logs) {
        sumExp += std::exp(log - maxLog);
    }
    return maxLog + std::log(sumExp);
}


static inline double distance(std::array<double,2> log1,
                              std::array<double,2> log2) {
    double ans = 0.0;
    for (uint64_t i = 0; i < 2; i++) {
        ans += std::abs(std::exp(log1[i]) - std::exp(log2[i]));
    }
    return ans;
}


static inline double distance_vl(std::array<double,2> val1,
                                 std::array<double,2> log2) {
    double ans = 0.0;
    for (uint64_t i = 0; i < 2; i++) {
        ans += std::abs(val1[i] - std::exp(log2[i]));
    }
    return ans;
}



} // namespace utils
