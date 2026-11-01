#include "backprop.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Workbench {

ForwardPass compute_forward(double x, double y, const NetworkParams& params) {
    ForwardPass fwd;
    fwd.x = x;
    fwd.y = y;
    fwd.z = params.w * x + params.b;
    fwd.h = std::tanh(fwd.z);
    fwd.y_hat = params.v * fwd.h + params.c;
    const double error = fwd.y_hat - y;
    fwd.loss = 0.5 * error * error;
    return fwd;
}

Gradients compute_analytical_gradients(const ForwardPass& fwd, const NetworkParams& params) {
    Gradients g;
    // dJ/dy_hat = (y_hat - y)
    g.dJ_dy_hat = (fwd.y_hat - fwd.y);

    // Layer 2 gradients: y_hat = v*h + c
    g.dJ_dv = g.dJ_dy_hat * fwd.h;
    g.dJ_dc = g.dJ_dy_hat * 1.0;

    // Backprop to hidden layer: dJ/dh = dJ/dy_hat * v
    g.dJ_dh = g.dJ_dy_hat * params.v;

    // Derivative of activation: d/dz tanh(z) = 1.0 - tanh^2(z) = 1.0 - h^2
    const double dtanh_dz = 1.0 - fwd.h * fwd.h;
    g.dJ_dz = g.dJ_dh * dtanh_dz;

    // Layer 1 gradients: z = w*x + b
    g.dJ_dw = g.dJ_dz * fwd.x;
    g.dJ_db = g.dJ_dz * 1.0;

    return g;
}

std::vector<GradientCheckRow> verify_gradients_finite_difference(
    double x, double y, const NetworkParams& params, double eps, double tolerance) {
    
    std::vector<GradientCheckRow> rows;
    const ForwardPass fwd = compute_forward(x, y, params);
    const Gradients g_anal = compute_analytical_gradients(fwd, params);

    // Helper lambda to compute loss for arbitrary parameters
    auto loss_for = [&](const NetworkParams& p) -> double {
        return compute_forward(x, y, p).loss;
    };

    // 1. Parameter w
    {
        NetworkParams p_plus = params;  p_plus.w += eps;
        NetworkParams p_minus = params; p_minus.w -= eps;
        const double g_num = (loss_for(p_plus) - loss_for(p_minus)) / (2.0 * eps);
        const double diff = std::abs(g_anal.dJ_dw - g_num);
        rows.push_back({"w", params.w, g_anal.dJ_dw, g_num, diff, (diff <= tolerance)});
    }

    // 2. Parameter b
    {
        NetworkParams p_plus = params;  p_plus.b += eps;
        NetworkParams p_minus = params; p_minus.b -= eps;
        const double g_num = (loss_for(p_plus) - loss_for(p_minus)) / (2.0 * eps);
        const double diff = std::abs(g_anal.dJ_db - g_num);
        rows.push_back({"b", params.b, g_anal.dJ_db, g_num, diff, (diff <= tolerance)});
    }

    // 3. Parameter v
    {
        NetworkParams p_plus = params;  p_plus.v += eps;
        NetworkParams p_minus = params; p_minus.v -= eps;
        const double g_num = (loss_for(p_plus) - loss_for(p_minus)) / (2.0 * eps);
        const double diff = std::abs(g_anal.dJ_dv - g_num);
        rows.push_back({"v", params.v, g_anal.dJ_dv, g_num, diff, (diff <= tolerance)});
    }

    // 4. Parameter c
    {
        NetworkParams p_plus = params;  p_plus.c += eps;
        NetworkParams p_minus = params; p_minus.c -= eps;
        const double g_num = (loss_for(p_plus) - loss_for(p_minus)) / (2.0 * eps);
        const double diff = std::abs(g_anal.dJ_dc - g_num);
        rows.push_back({"c", params.c, g_anal.dJ_dc, g_num, diff, (diff <= tolerance)});
    }

    return rows;
}

TrainingSimulationResult run_backpropagation_simulation(
    double x, double y, NetworkParams init_p, double lr, int epochs) {

    TrainingSimulationResult sim;
    sim.initial_params = init_p;
    sim.learning_rate = lr;
    sim.total_epochs = epochs;

    // Initial gradient check
    sim.initial_grad_check = verify_gradients_finite_difference(x, y, init_p, 1.0e-6, 1.0e-5);
    sim.all_grad_checks_passed = true;
    for (const auto& row : sim.initial_grad_check) {
        if (!row.passed) {
            sim.all_grad_checks_passed = false;
        }
    }

    NetworkParams current_p = init_p;

    // Record Epoch 0 (Initial condition before any gradient descent steps)
    {
        EpochRecord rec;
        rec.epoch = 0;
        rec.params = current_p;
        rec.forward = compute_forward(x, y, current_p);
        rec.grads = compute_analytical_gradients(rec.forward, current_p);
        sim.history.push_back(rec);
    }

    // Gradient descent training loop
    for (int ep = 1; ep <= epochs; ++ep) {
        const ForwardPass fwd = compute_forward(x, y, current_p);
        const Gradients g = compute_analytical_gradients(fwd, current_p);

        // Gradient descent parameter updates: theta <- theta - lr * grad
        current_p.w -= lr * g.dJ_dw;
        current_p.b -= lr * g.dJ_db;
        current_p.v -= lr * g.dJ_dv;
        current_p.c -= lr * g.dJ_dc;

        EpochRecord rec;
        rec.epoch = ep;
        rec.params = current_p;
        rec.forward = compute_forward(x, y, current_p);
        rec.grads = compute_analytical_gradients(rec.forward, current_p);
        sim.history.push_back(rec);
    }

    return sim;
}

std::string export_training_to_csv(const TrainingSimulationResult& sim) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "Epoch,w,b,v,c,z,h,y_hat,target_y,loss_J,grad_w,grad_b,grad_v,grad_c\n";
    for (const auto& rec : sim.history) {
        ss << rec.epoch << ","
           << rec.params.w << ","
           << rec.params.b << ","
           << rec.params.v << ","
           << rec.params.c << ","
           << rec.forward.z << ","
           << rec.forward.h << ","
           << rec.forward.y_hat << ","
           << rec.forward.y << ","
           << rec.forward.loss << ","
           << rec.grads.dJ_dw << ","
           << rec.grads.dJ_db << ","
           << rec.grads.dJ_dv << ","
           << rec.grads.dJ_dc << "\n";
    }
    return ss.str();
}

} // namespace Workbench
