#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace utils {

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

namespace utils_CSR {
    static inline double logSum(const std::vector<double>& logs) {
        constexpr double NINF = -std::numeric_limits<double>::infinity();
        double maxlog = NINF;
        for (double log : logs) {
            maxlog = std::max(maxlog, log);
        }
        if (maxlog == NINF) return NINF;

        double sumExp = 0.0;
        for (double x : logs) {
            sumExp += std::exp(x - maxlog);
        }
        return maxlog + std::log(sumExp);
    }

    template <size_t N>
    static inline double varLogSum(const std::array<double, N>& logs) {
        double maxVal = logs[0];
        for (size_t i = 1; i < N; ++i) {
            if (logs[i] > maxVal) maxVal = logs[i];
        }
    
        double sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += std::exp(logs[i] - maxVal);
        }
    
        return maxVal + std::log(sum);
    }

    static inline double logDifference(const std::vector<double>& log1, const std::vector<double>& log2) {
        double ans = 0.0;
        assert(log1.size() == log2.size());
        for (uint64_t i = 0; i<log1.size(); i++) {
            ans += std::abs(std::exp(log1[i]) - std::exp(log2[i]));
        }


        return ans;
    }

} // namespace utils_CSR
