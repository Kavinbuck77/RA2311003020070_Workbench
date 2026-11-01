#ifndef FEA_SOLVER_HPP
#define FEA_SOLVER_HPP

#include <vector>
#include <string>

namespace Workbench {

struct ThermalParameters {
    double L = 1.0;          // Length of the rod [m]
    double A = 1.0e-4;       // Cross-sectional area [m^2]
    double k = 10.0;         // Thermal conductivity [W/(m·K)]
    double q = 10000.0;      // Volumetric heat generation rate [W/m^3]
    double T0 = 100.0;       // Prescribed Dirichlet BC at x = 0 [°C]
    double TL = 20.0;        // Prescribed Dirichlet BC at x = L [°C]
};

struct FeaResult {
    int num_elements = 0;
    int num_nodes = 0;
    double h = 0.0; // Element length [m]

    // Nodal values
    std::vector<double> node_x;
    std::vector<double> node_T_fea;
    std::vector<double> node_T_exact;
    std::vector<double> node_abs_error;

    // Midpoint values (where linear FEA exhibits true discretization error)
    std::vector<double> midpoint_x;
    std::vector<double> midpoint_T_fea;
    std::vector<double> midpoint_T_exact;
    std::vector<double> midpoint_abs_error;

    double max_midpoint_error = 0.0;
    double l2_midpoint_error = 0.0;
    double execution_time_us = 0.0;
};

struct ConvergenceRow {
    int N = 0;
    double h = 0.0;
    double max_error = 0.0;
    double l2_error = 0.0;
    double convergence_rate = 0.0; // Estimated rate alpha: Error ~ C * h^alpha
};

struct ConvergenceStudy {
    ThermalParameters params;
    std::vector<FeaResult> results;
    std::vector<ConvergenceRow> table;
};

// Analytical exact solution: T_exact(x) = T0 - (T0 - TL)*(x/L) + (q / (2.0*k)) * x * (L - x)
double analytical_temperature(double x, const ThermalParameters& params);

// 1D Linear Finite Element Solver
FeaResult solve_1d_thermal(int num_elements, const ThermalParameters& params);

// Run multi-resolution convergence study across specified element counts (e.g., 10, 20, 40, 80)
ConvergenceStudy run_convergence_study(const std::vector<int>& element_counts, const ThermalParameters& params);

// Export convergence study results to CSV format
std::string export_convergence_to_csv(const ConvergenceStudy& study);

} // namespace Workbench

#endif // FEA_SOLVER_HPP
