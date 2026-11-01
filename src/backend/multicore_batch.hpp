#ifndef MULTICORE_BATCH_HPP
#define MULTICORE_BATCH_HPP

#include "fea_solver.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace Workbench {

struct BatchCase {
    int id = 0;
    double k = 10.0;       // W/(m·K)
    double q = 10000.0;    // W/m^3
    double L = 1.0;
    double A = 1.0e-4;
    double T0 = 100.0;
    double TL = 20.0;
    int num_elements = 80; // High element count for realistic compute workload
};

struct BatchSolveSummary {
    int case_id = 0;
    double k = 0.0;
    double q = 0.0;
    double T_midpoint = 0.0;
    double T_max = 0.0;
    double max_error = 0.0;
};

struct BenchmarkRun {
    int num_threads = 1;
    double warmup_time_ms = 0.0;
    std::vector<double> iteration_times_ms; // 5 timed iterations
    double median_time_ms = 0.0;
    double mean_time_ms = 0.0;
    double min_time_ms = 0.0;
    double max_time_ms = 0.0;
    double speedup = 1.0;                  // S_p = T_1 / T_p
    double efficiency = 1.0;               // E_p = S_p / p
    double max_discrepancy_vs_serial = 0.0;
    bool verification_passed = false;     // Assert <= 1e-8
};

struct BatchBenchmarkReport {
    size_t num_cases = 0;
    uint32_t seed = 42;
    int elements_per_solve = 80;
    std::vector<BenchmarkRun> runs; // p = 1, 2, 4
    bool all_verified = false;
};

// Deterministic batch generator using mt19937
std::vector<BatchCase> generate_deterministic_cases(size_t num_cases, uint32_t seed = 42, int elements_per_solve = 80);

// Parallel execution worker pool
std::vector<BatchSolveSummary> execute_batch_parallel(const std::vector<BatchCase>& cases, int num_threads);

// Full benchmark suite with warmup, 5 iterations, median timing, speedup & efficiency calculation, and serial discrepancy verification
BatchBenchmarkReport run_multicore_benchmark(size_t num_cases = 1000, uint32_t seed = 42, const std::vector<int>& thread_configs = {1, 2, 4});

// Export benchmark data to CSV format
std::string export_benchmark_to_csv(const BatchBenchmarkReport& report);

} // namespace Workbench

#endif // MULTICORE_BATCH_HPP
