#include "multicore_batch.hpp"
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Workbench {

std::vector<BatchCase> generate_deterministic_cases(size_t num_cases, uint32_t seed, int elements_per_solve) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist_k(5.0, 50.0);       // Thermal conductivity [5.0, 50.0] W/(m·K)
    std::uniform_real_distribution<double> dist_q(1000.0, 50000.0); // Heat generation [1000, 50000] W/m^3

    std::vector<BatchCase> cases(num_cases);
    for (size_t i = 0; i < num_cases; ++i) {
        cases[i].id = static_cast<int>(i);
        cases[i].k = dist_k(rng);
        cases[i].q = dist_q(rng);
        cases[i].L = 1.0;
        cases[i].A = 1.0e-4;
        cases[i].T0 = 100.0;
        cases[i].TL = 20.0;
        cases[i].num_elements = elements_per_solve;
    }
    return cases;
}

std::vector<BatchSolveSummary> execute_batch_parallel(const std::vector<BatchCase>& cases, int num_threads) {
    const size_t total_cases = cases.size();
    std::vector<BatchSolveSummary> results(total_cases);

    if (total_cases == 0) return results;

    if (num_threads <= 1) {
        // Serial execution
        for (size_t i = 0; i < total_cases; ++i) {
            ThermalParameters p;
            p.L = cases[i].L;
            p.A = cases[i].A;
            p.k = cases[i].k;
            p.q = cases[i].q;
            p.T0 = cases[i].T0;
            p.TL = cases[i].TL;

            FeaResult res = solve_1d_thermal(cases[i].num_elements, p);
            results[i].case_id = cases[i].id;
            results[i].k = cases[i].k;
            results[i].q = cases[i].q;
            results[i].T_midpoint = res.midpoint_T_fea[res.midpoint_T_fea.size() / 2];
            results[i].T_max = *std::max_element(res.node_T_fea.begin(), res.node_T_fea.end());
            results[i].max_error = res.max_midpoint_error;
        }
        return results;
    }

    // Parallel execution with dynamic chunked atomic scheduling
    std::atomic<size_t> next_index{0};
    const size_t CHUNK_SIZE = 16;
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    auto worker_fn = [&]() {
        while (true) {
            size_t start_idx = next_index.fetch_add(CHUNK_SIZE, std::memory_order_relaxed);
            if (start_idx >= total_cases) break;
            size_t end_idx = std::min(start_idx + CHUNK_SIZE, total_cases);

            for (size_t i = start_idx; i < end_idx; ++i) {
                ThermalParameters p;
                p.L = cases[i].L;
                p.A = cases[i].A;
                p.k = cases[i].k;
                p.q = cases[i].q;
                p.T0 = cases[i].T0;
                p.TL = cases[i].TL;

                FeaResult res = solve_1d_thermal(cases[i].num_elements, p);
                results[i].case_id = cases[i].id;
                results[i].k = cases[i].k;
                results[i].q = cases[i].q;
                results[i].T_midpoint = res.midpoint_T_fea[res.midpoint_T_fea.size() / 2];
                results[i].T_max = *std::max_element(res.node_T_fea.begin(), res.node_T_fea.end());
                results[i].max_error = res.max_midpoint_error;
            }
        }
    };

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back(worker_fn);
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    return results;
}

BatchBenchmarkReport run_multicore_benchmark(size_t num_cases, uint32_t seed, const std::vector<int>& thread_configs) {
    BatchBenchmarkReport report;
    report.num_cases = num_cases;
    report.seed = seed;
    report.elements_per_solve = 80;
    report.all_verified = true;

    // 1. Generate identical deterministic test set
    const std::vector<BatchCase> cases = generate_deterministic_cases(num_cases, seed, report.elements_per_solve);

    // 2. Compute canonical serial reference solution for verification
    const std::vector<BatchSolveSummary> serial_reference = execute_batch_parallel(cases, 1);

    double baseline_T1_median = 0.0;

    for (int threads : thread_configs) {
        BenchmarkRun run;
        run.num_threads = threads;

        // Warmup run (discarded)
        const auto t_warmup_start = std::chrono::high_resolution_clock::now();
        execute_batch_parallel(cases, threads);
        const auto t_warmup_end = std::chrono::high_resolution_clock::now();
        run.warmup_time_ms = std::chrono::duration<double, std::milli>(t_warmup_end - t_warmup_start).count();

        // 5 timed benchmark iterations
        run.iteration_times_ms.resize(5);
        std::vector<BatchSolveSummary> last_parallel_results;

        for (int iter = 0; iter < 5; ++iter) {
            const auto t_start = std::chrono::high_resolution_clock::now();
            last_parallel_results = execute_batch_parallel(cases, threads);
            const auto t_end = std::chrono::high_resolution_clock::now();
            run.iteration_times_ms[iter] = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        }

        // Compute median, mean, min, max
        std::vector<double> sorted_times = run.iteration_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        run.median_time_ms = sorted_times[2]; // Median of 5 iterations
        run.min_time_ms = sorted_times[0];
        run.max_time_ms = sorted_times[4];
        run.mean_time_ms = std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0) / 5.0;

        if (threads == 1) {
            baseline_T1_median = run.median_time_ms;
            run.speedup = 1.0;
            run.efficiency = 1.0;
            run.max_discrepancy_vs_serial = 0.0;
            run.verification_passed = true;
        } else {
            run.speedup = (run.median_time_ms > 0.0) ? (baseline_T1_median / run.median_time_ms) : 1.0;
            run.efficiency = run.speedup / static_cast<double>(threads);

            // Numerical equivalence verification: max |parallel - serial| <= 1e-8
            double max_diff = 0.0;
            for (size_t i = 0; i < cases.size(); ++i) {
                const double diff_mid = std::abs(last_parallel_results[i].T_midpoint - serial_reference[i].T_midpoint);
                const double diff_max = std::abs(last_parallel_results[i].T_max - serial_reference[i].T_max);
                const double diff_err = std::abs(last_parallel_results[i].max_error - serial_reference[i].max_error);
                max_diff = std::max({max_diff, diff_mid, diff_max, diff_err});
            }
            run.max_discrepancy_vs_serial = max_diff;
            run.verification_passed = (max_diff <= 1.0e-8);
            if (!run.verification_passed) {
                report.all_verified = false;
            }
        }

        report.runs.push_back(run);
    }

    return report;
}

std::string export_benchmark_to_csv(const BatchBenchmarkReport& report) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "Threads_p,Warmup_ms,Median_ms,Mean_ms,Min_ms,Max_ms,Speedup_Sp,Efficiency_Ep,Max_Discrepancy_vs_Serial,Verification_Status\n";
    for (const auto& run : report.runs) {
        ss << run.num_threads << ","
           << run.warmup_time_ms << ","
           << run.median_time_ms << ","
           << run.mean_time_ms << ","
           << run.min_time_ms << ","
           << run.max_time_ms << ","
           << run.speedup << ","
           << run.efficiency << ","
           << std::scientific << run.max_discrepancy_vs_serial << std::fixed << ","
           << (run.verification_passed ? "PASS" : "FAIL") << "\n";
    }
    return ss.str();
}

} // namespace Workbench
