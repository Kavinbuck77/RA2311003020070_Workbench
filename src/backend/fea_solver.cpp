#include "fea_solver.hpp"
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Workbench {

double analytical_temperature(double x, const ThermalParameters& params) {
    // Exact solution to: -k * d^2T/dx^2 = q, with T(0) = T0, T(L) = TL
    // T_exact(x) = T0 - (T0 - TL)*(x/L) + (q / (2*k)) * x * (L - x)
    if (params.L <= 0.0 || params.k <= 0.0) return 0.0;
    const double linear_part = params.T0 - (params.T0 - params.TL) * (x / params.L);
    const double quadratic_part = (params.q / (2.0 * params.k)) * x * (params.L - x);
    return linear_part + quadratic_part;
}

FeaResult solve_1d_thermal(int num_elements, const ThermalParameters& params) {
    const auto t_start = std::chrono::high_resolution_clock::now();

    FeaResult result;
    if (num_elements < 1) return result;

    result.num_elements = num_elements;
    result.num_nodes = num_elements + 1;
    result.h = params.L / static_cast<double>(num_elements);

    const int N = num_elements;
    const double h = result.h;
    const double k_coeff = (params.k * params.A) / h;
    const double f_interior = params.q * params.A * h;

    // Node coordinates
    result.node_x.resize(result.num_nodes);
    result.node_T_fea.resize(result.num_nodes, 0.0);
    result.node_T_exact.resize(result.num_nodes, 0.0);
    result.node_abs_error.resize(result.num_nodes, 0.0);

    for (int i = 0; i < result.num_nodes; ++i) {
        result.node_x[i] = i * h;
        result.node_T_exact[i] = analytical_temperature(result.node_x[i], params);
    }

    // Apply Dirichlet Boundary Conditions at boundaries
    result.node_T_fea[0] = params.T0;
    result.node_T_fea[N] = params.TL;

    if (N == 1) {
        // Special case: single element, both ends are fixed boundary conditions
        // Node 0 and Node 1 are fully determined
    } else {
        // Tridiagonal system for interior nodes 1 to N-1
        // Size M = N - 1
        const int M = N - 1;
        std::vector<double> a(M, -k_coeff);       // Sub-diagonal
        std::vector<double> b(M, 2.0 * k_coeff); // Main diagonal
        std::vector<double> c(M, -k_coeff);       // Super-diagonal
        std::vector<double> d(M, f_interior);    // Right-hand side

        // Incorporate Dirichlet boundary conditions into RHS
        // Node 1 receives contribution from Node 0
        d[0] += k_coeff * params.T0;
        // Node N-1 receives contribution from Node N
        d[M - 1] += k_coeff * params.TL;

        // Thomas Algorithm (Tridiagonal Matrix Algorithm - TDMA)
        std::vector<double> c_prime(M, 0.0);
        std::vector<double> d_prime(M, 0.0);

        c_prime[0] = c[0] / b[0];
        d_prime[0] = d[0] / b[0];

        for (int i = 1; i < M; ++i) {
            const double m = 1.0 / (b[i] - a[i] * c_prime[i - 1]);
            c_prime[i] = c[i] * m;
            d_prime[i] = (d[i] - a[i] * d_prime[i - 1]) * m;
        }

        // Back substitution
        std::vector<double> T_interior(M, 0.0);
        T_interior[M - 1] = d_prime[M - 1];
        for (int i = M - 2; i >= 0; --i) {
            T_interior[i] = d_prime[i] - c_prime[i] * T_interior[i + 1];
        }

        // Populate global nodal solution
        for (int i = 0; i < M; ++i) {
            result.node_T_fea[i + 1] = T_interior[i];
        }
    }

    // Compute nodal errors
    for (int i = 0; i < result.num_nodes; ++i) {
        result.node_abs_error[i] = std::abs(result.node_T_fea[i] - result.node_T_exact[i]);
    }

    // Midpoint calculations (Linear FEA discretization error evaluation)
    result.midpoint_x.resize(N);
    result.midpoint_T_fea.resize(N);
    result.midpoint_T_exact.resize(N);
    result.midpoint_abs_error.resize(N);

    double max_err = 0.0;
    double sum_sq_err = 0.0;

    for (int e = 0; e < N; ++e) {
        const double x_mid = 0.5 * (result.node_x[e] + result.node_x[e + 1]);
        const double T_fea_mid = 0.5 * (result.node_T_fea[e] + result.node_T_fea[e + 1]);
        const double T_exact_mid = analytical_temperature(x_mid, params);
        const double err = std::abs(T_fea_mid - T_exact_mid);

        result.midpoint_x[e] = x_mid;
        result.midpoint_T_fea[e] = T_fea_mid;
        result.midpoint_T_exact[e] = T_exact_mid;
        result.midpoint_abs_error[e] = err;

        if (err > max_err) max_err = err;
        sum_sq_err += err * err;
    }

    result.max_midpoint_error = max_err;
    result.l2_midpoint_error = std::sqrt(sum_sq_err * h);

    const auto t_end = std::chrono::high_resolution_clock::now();
    result.execution_time_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();

    return result;
}

ConvergenceStudy run_convergence_study(const std::vector<int>& element_counts, const ThermalParameters& params) {
    ConvergenceStudy study;
    study.params = params;

    for (size_t i = 0; i < element_counts.size(); ++i) {
        const int N = element_counts[i];
        FeaResult res = solve_1d_thermal(N, params);
        study.results.push_back(res);

        ConvergenceRow row;
        row.N = N;
        row.h = res.h;
        row.max_error = res.max_midpoint_error;
        row.l2_error = res.l2_midpoint_error;

        if (i > 0) {
            const double h_prev = study.table[i - 1].h;
            const double err_prev = study.table[i - 1].max_error;
            // Convergence rate alpha = log(E_prev / E_curr) / log(h_prev / h_curr)
            if (res.max_midpoint_error > 0.0 && err_prev > 0.0) {
                row.convergence_rate = std::log(err_prev / res.max_midpoint_error) / std::log(h_prev / res.h);
            } else {
                row.convergence_rate = 0.0;
            }
        } else {
            row.convergence_rate = 0.0; // Baseline
        }

        study.table.push_back(row);
    }

    return study;
}

std::string export_convergence_to_csv(const ConvergenceStudy& study) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "Elements_N,Mesh_Size_h,Max_Midpoint_Error_C,L2_Midpoint_Error_C,Convergence_Rate_alpha\n";
    for (const auto& row : study.table) {
        ss << row.N << ","
           << row.h << ","
           << row.max_error << ","
           << row.l2_error << ","
           << row.convergence_rate << "\n";
    }
    return ss.str();
}

} // namespace Workbench
