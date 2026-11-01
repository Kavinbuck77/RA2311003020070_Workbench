#ifndef BACKPROP_HPP
#define BACKPROP_HPP

#include <vector>
#include <string>

namespace Workbench {

struct NetworkParams {
    double w = 0.5; // Input-to-hidden weight
    double b = 0.0; // Hidden bias
    double v = 1.0; // Hidden-to-output weight
    double c = 0.0; // Output bias
};

struct ForwardPass {
    double x = 1.0;     // Input
    double y = 2.0;     // Target
    double z = 0.0;     // Pre-activation: z = w*x + b
    double h = 0.0;     // Hidden activation: h = tanh(z)
    double y_hat = 0.0; // Output: y_hat = v*h + c
    double loss = 0.0;  // Half-squared error loss: J = 0.5 * (y_hat - y)^2
};

struct Gradients {
    double dJ_dy_hat = 0.0; // dJ/dy_hat = (y_hat - y)
    double dJ_dv = 0.0;     // dJ/dv = dJ/dy_hat * h
    double dJ_dc = 0.0;     // dJ/dc = dJ/dy_hat * 1.0
    double dJ_dh = 0.0;     // dJ/dh = dJ/dy_hat * v
    double dJ_dz = 0.0;     // dJ/dz = dJ/dh * (1 - h^2)
    double dJ_dw = 0.0;     // dJ/dw = dJ/dz * x
    double dJ_db = 0.0;     // dJ/db = dJ/dz * 1.0
};

struct GradientCheckRow {
    std::string param_name;
    double param_val = 0.0;
    double analytical_grad = 0.0;
    double numerical_grad = 0.0;
    double abs_error = 0.0;
    bool passed = false; // Verified <= 1e-5
};

struct EpochRecord {
    int epoch = 0;
    NetworkParams params;
    ForwardPass forward;
    Gradients grads;
};

struct TrainingSimulationResult {
    NetworkParams initial_params;
    double learning_rate = 0.1;
    int total_epochs = 100;
    std::vector<GradientCheckRow> initial_grad_check;
    std::vector<EpochRecord> history;
    bool all_grad_checks_passed = false;
};

// Forward pass computation
ForwardPass compute_forward(double x, double y, const NetworkParams& params);

// Analytical gradient computation via the chain rule
Gradients compute_analytical_gradients(const ForwardPass& fwd, const NetworkParams& params);

// Finite difference numerical gradient check: [J(p + eps) - J(p - eps)] / (2*eps)
std::vector<GradientCheckRow> verify_gradients_finite_difference(
    double x, double y, const NetworkParams& params, double eps = 1.0e-6, double tolerance = 1.0e-5);

// Train the network for N epochs (default 100) using Gradient Descent
TrainingSimulationResult run_backpropagation_simulation(
    double x = 1.0, double y = 2.0, NetworkParams init_p = {0.5, 0.0, 1.0, 0.0}, double lr = 0.1, int epochs = 100);

// Export training history to CSV
std::string export_training_to_csv(const TrainingSimulationResult& sim);

} // namespace Workbench

#endif // BACKPROP_HPP
