// perf_metrics.h

#ifndef PERF_METRICS_H
#define PERF_METRICS_H

// Structure to hold performance metrics
struct perf_metrics {
    double runtime_ms;  // Total runtime in milliseconds
    double num_updates; // Number of updates performed
    double num_skips;   // Number of skips performed (if any)
};

#endif // PERF_METRICS_H
