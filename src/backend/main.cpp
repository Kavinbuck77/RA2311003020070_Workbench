#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <filesystem>
#include <cmath>

#include "http_server.hpp"
#include "fea_solver.hpp"
#include "multicore_batch.hpp"
#include "embedded_mock.hpp"
#include "backprop.hpp"
#include "cad_ansys_parser.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
using namespace Workbench;

// Helper to read file to string
static std::string read_file_contents(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    // 0. Ensure working directory is the folder where the executable resides
    if (argc > 0 && argv[0] != nullptr) {
        try {
            fs::path exe_dir = fs::absolute(argv[0]).parent_path();
            if (fs::exists(exe_dir / "src" / "frontend" / "index.html")) {
                fs::current_path(exe_dir);
            }
        } catch (...) {}
    }

    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            port = 8080;
        }
    }

    std::cout << "=================================================================\n";
    std::cout << "  PASS SCIENCE WORKS R&D INTERNSHIP 2026-2027\n";
    std::cout << "  Unified Engineering Workbench Native C++ Engine\n";
    std::cout << "  Candidate: KAVIN K K (RA2311003020070) - Principal Systems & Scientific Software Engineer\n";
    std::cout << "=================================================================\n" << std::endl;

    // 1. Ensure data files exist, or create them from inline generators
    fs::create_directories("data");
    fs::create_directories("results/raw_csv");
    fs::create_directories("results/plots");

    std::string stl_path = "data/sample.stl";
    if (!fs::exists(stl_path)) {
        std::ofstream stl_out(stl_path);
        stl_out << generate_synthetic_stl();
        stl_out.close();
        std::cout << "[Storage] Initialized sample CAD STL at: " << stl_path << std::endl;
    }

    std::string ansys_path = "data/ansys_nodes.csv";
    if (!fs::exists(ansys_path)) {
        std::ofstream ansys_out(ansys_path);
        ansys_out << generate_synthetic_ansys_csv();
        ansys_out.close();
        std::cout << "[Storage] Initialized sample ANSYS CSV at: " << ansys_path << std::endl;
    }

    // 2. Pre-generate and verify baseline results to results/raw_csv/
    {
        // Task C Baseline Solve
        ThermalParameters default_params;
        ConvergenceStudy study = run_convergence_study({10, 20, 40, 80}, default_params);
        std::ofstream fea_csv("results/raw_csv/fea_convergence.csv");
        fea_csv << export_convergence_to_csv(study);
        fea_csv.close();
        std::cout << "[FEA Engine] Baseline convergence study computed (N=10,20,40,80). Max error N=80: "
                  << study.table.back().max_error << " C. Rate alpha: "
                  << study.table.back().convergence_rate << std::endl;

        // Task D Baseline Benchmark
        BatchBenchmarkReport bench = run_multicore_benchmark(1000, 42, {1, 2, 4});
        std::ofstream bench_csv("results/raw_csv/multicore_benchmark.csv");
        bench_csv << export_benchmark_to_csv(bench);
        bench_csv.close();
        std::cout << "[Multicore Engine] 1000-solve batch benchmark complete across 1, 2, 4 threads.\n";
        for (const auto& r : bench.runs) {
            std::cout << "  Threads: " << r.num_threads << " | Median: " << r.median_time_ms
                      << " ms | Speedup: " << r.speedup << "x | Efficiency: " << (r.efficiency * 100.0)
                      << "% | Discrepancy: " << r.max_discrepancy_vs_serial << "\n";
        }

        // Task F Baseline Backpropagation
        TrainingSimulationResult bp_sim = run_backpropagation_simulation(1.0, 2.0, {0.5, 0.0, 1.0, 0.0}, 0.1, 100);
        std::ofstream bp_csv("results/raw_csv/backprop_training.csv");
        bp_csv << export_training_to_csv(bp_sim);
        bp_csv.close();
        std::cout << "[Neural Engine] Gradient check verified (all <= 1e-5: "
                  << (bp_sim.all_grad_checks_passed ? "PASS" : "FAIL") << "). Initial Loss: "
                  << bp_sim.history.front().forward.loss << " -> Final Loss: "
                  << bp_sim.history.back().forward.loss << std::endl;
    }

    // 3. Embedded Subsystem instance
    auto embedded_sys = std::make_shared<EmbeddedWorkbenchSubsystem>();
    embedded_sys->execute_full_integration_sequence();

    // 4. Initialize HTTP Server
    SimpleHttpServer server(port);

    // Route: GET /
    server.Get("/", [](const HttpRequest& req) -> HttpResponse {
        // Look for frontend file in common relative locations
        std::vector<std::string> search_paths = {
            "src/frontend/index.html",
            "../src/frontend/index.html",
            "frontend/index.html",
            "index.html"
        };
        for (const auto& p : search_paths) {
            if (fs::exists(p)) {
                return HttpResponse::Html(read_file_contents(p));
            }
        }
        return HttpResponse::NotFound("index.html not found. Check src/frontend/index.html path.");
    });

    // Route: Static files & Direct Downloads
    server.Get("/sample.stl", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/sample.stl");
        if (s.empty()) s = generate_synthetic_stl();
        HttpResponse r;
        r.content_type = "model/stl";
        r.body = s;
        return r;
    });

    server.Get("/data/sample.stl", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/sample.stl");
        if (s.empty()) s = generate_synthetic_stl();
        HttpResponse r;
        r.content_type = "model/stl";
        r.body = s;
        return r;
    });

    server.Get("/ansys_nodes.csv", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/ansys_nodes.csv");
        if (s.empty()) s = generate_synthetic_ansys_csv();
        return HttpResponse::Csv(s, "ansys_nodes.csv");
    });

    server.Get("/data/ansys_nodes.csv", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/ansys_nodes.csv");
        if (s.empty()) s = generate_synthetic_ansys_csv();
        return HttpResponse::Csv(s, "ansys_nodes.csv");
    });

    server.Get("/api/download/sample.stl", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/sample.stl");
        if (s.empty()) s = generate_synthetic_stl();
        HttpResponse r;
        r.status_code = 200;
        r.content_type = "model/stl; charset=utf-8";
        r.headers["Content-Disposition"] = "attachment; filename=\"sample.stl\"";
        r.body = s;
        return r;
    });

    server.Get("/api/download/ansys_nodes.csv", [](const HttpRequest&) -> HttpResponse {
        std::string s = read_file_contents("data/ansys_nodes.csv");
        if (s.empty()) s = generate_synthetic_ansys_csv();
        return HttpResponse::Csv(s, "ansys_nodes.csv");
    });

    // -------------------------------------------------------------
    // REST API Endpoints
    // -------------------------------------------------------------

    // 1. CAD & ANSYS API
    server.Get("/api/cad/stl", [](const HttpRequest&) -> HttpResponse {
        StlModel model = parse_ascii_stl_file("data/sample.stl");
        return HttpResponse::Json(stl_model_to_json(model));
    });

    server.Get("/api/cad/ansys", [](const HttpRequest&) -> HttpResponse {
        AnsysDataset ds = parse_ansys_csv_file("data/ansys_nodes.csv");
        return HttpResponse::Json(ansys_dataset_to_json(ds));
    });

    // 2. FEA Thermal Solve API
    server.Get("/api/fea/solve", [](const HttpRequest& req) -> HttpResponse {
        ThermalParameters p;
        try {
            if (!req.get_param("L").empty()) p.L = std::stod(req.get_param("L"));
            if (!req.get_param("A").empty()) p.A = std::stod(req.get_param("A"));
            if (!req.get_param("k").empty()) p.k = std::stod(req.get_param("k"));
            if (!req.get_param("q").empty()) p.q = std::stod(req.get_param("q"));
            if (!req.get_param("T0").empty()) p.T0 = std::stod(req.get_param("T0"));
            if (!req.get_param("TL").empty()) p.TL = std::stod(req.get_param("TL"));
        } catch (...) {
            return HttpResponse::Json("{\"error\":\"Invalid numeric parameters provided\"}", 400);
        }

        ConvergenceStudy study = run_convergence_study({10, 20, 40, 80}, p);

        std::ostringstream ss;
        ss << "{\"params\":{"
           << "\"L\":" << p.L << ",\"A\":" << p.A << ",\"k\":" << p.k
           << ",\"q\":" << p.q << ",\"T0\":" << p.T0 << ",\"TL\":" << p.TL
           << "},\"convergence_table\":[";

        for (size_t i = 0; i < study.table.size(); ++i) {
            const auto& row = study.table[i];
            ss << "{\"N\":" << row.N
               << ",\"h\":" << row.h
               << ",\"max_error\":" << row.max_error
               << ",\"l2_error\":" << row.l2_error
               << ",\"convergence_rate\":" << row.convergence_rate << "}";
            if (i + 1 < study.table.size()) ss << ",";
        }
        ss << "],\"solves\":[";

        for (size_t s = 0; s < study.results.size(); ++s) {
            const auto& res = study.results[s];
            ss << "{\"N\":" << res.num_elements
               << ",\"h\":" << res.h
               << ",\"max_midpoint_error\":" << res.max_midpoint_error
               << ",\"exec_time_us\":" << res.execution_time_us
               << ",\"nodes_x\":[";
            for (size_t i = 0; i < res.node_x.size(); ++i) {
                ss << res.node_x[i]; if (i + 1 < res.node_x.size()) ss << ",";
            }
            ss << "],\"nodes_T_fea\":[";
            for (size_t i = 0; i < res.node_T_fea.size(); ++i) {
                ss << res.node_T_fea[i]; if (i + 1 < res.node_T_fea.size()) ss << ",";
            }
            ss << "],\"nodes_T_exact\":[";
            for (size_t i = 0; i < res.node_T_exact.size(); ++i) {
                ss << res.node_T_exact[i]; if (i + 1 < res.node_T_exact.size()) ss << ",";
            }
            ss << "],\"midpoints_x\":[";
            for (size_t i = 0; i < res.midpoint_x.size(); ++i) {
                ss << res.midpoint_x[i]; if (i + 1 < res.midpoint_x.size()) ss << ",";
            }
            ss << "],\"midpoints_T_fea\":[";
            for (size_t i = 0; i < res.midpoint_T_fea.size(); ++i) {
                ss << res.midpoint_T_fea[i]; if (i + 1 < res.midpoint_T_fea.size()) ss << ",";
            }
            ss << "],\"midpoints_T_exact\":[";
            for (size_t i = 0; i < res.midpoint_T_exact.size(); ++i) {
                ss << res.midpoint_T_exact[i]; if (i + 1 < res.midpoint_T_exact.size()) ss << ",";
            }
            ss << "],\"midpoints_abs_error\":[";
            for (size_t i = 0; i < res.midpoint_abs_error.size(); ++i) {
                ss << res.midpoint_abs_error[i]; if (i + 1 < res.midpoint_abs_error.size()) ss << ",";
            }
            ss << "]}";
            if (s + 1 < study.results.size()) ss << ",";
        }
        ss << "]}";

        return HttpResponse::Json(ss.str());
    });

    // 3. Multicore Batch Benchmark API
    server.Get("/api/batch/benchmark", [](const HttpRequest& req) -> HttpResponse {
        size_t cases = 1000;
        uint32_t seed = 42;
        try {
            if (!req.get_param("cases").empty()) cases = std::stoul(req.get_param("cases"));
            if (!req.get_param("seed").empty()) seed = static_cast<uint32_t>(std::stoul(req.get_param("seed")));
        } catch (...) {
            cases = 1000;
            seed = 42;
        }

        BatchBenchmarkReport rep = run_multicore_benchmark(cases, seed, {1, 2, 4});

        std::ostringstream ss;
        ss << "{\"cases\":" << rep.num_cases
           << ",\"seed\":" << rep.seed
           << ",\"elements_per_solve\":" << rep.elements_per_solve
           << ",\"all_verified\":" << (rep.all_verified ? "true" : "false")
           << ",\"runs\":[";

        for (size_t i = 0; i < rep.runs.size(); ++i) {
            const auto& r = rep.runs[i];
            ss << "{\"threads\":" << r.num_threads
               << ",\"warmup_ms\":" << r.warmup_time_ms
               << ",\"median_ms\":" << r.median_time_ms
               << ",\"mean_ms\":" << r.mean_time_ms
               << ",\"min_ms\":" << r.min_time_ms
               << ",\"max_ms\":" << r.max_time_ms
               << ",\"speedup\":" << r.speedup
               << ",\"efficiency\":" << r.efficiency
               << ",\"discrepancy\":" << r.max_discrepancy_vs_serial
               << ",\"verified\":" << (r.verification_passed ? "true" : "false")
               << ",\"iterations_ms\":[";
            for (size_t j = 0; j < r.iteration_times_ms.size(); ++j) {
                ss << r.iteration_times_ms[j];
                if (j + 1 < r.iteration_times_ms.size()) ss << ",";
            }
            ss << "]}";
            if (i + 1 < rep.runs.size()) ss << ",";
        }
        ss << "]}";

        return HttpResponse::Json(ss.str());
    });

    // 4. Embedded Hardware State & Action API
    server.Get("/api/embedded/state", [embedded_sys](const HttpRequest&) -> HttpResponse {
        std::ostringstream ss;
        ss << "{\"devices\":[";
        const auto& dev_map = embedded_sys->get_devices();
        size_t count = 0;
        for (const auto& kv : dev_map) {
            const auto& dev = kv.second;
            const auto& desc = dev.get_descriptor();
            const auto& telem = dev.get_telemetry();

            ss << "{\"device_id\":" << static_cast<int>(dev.get_device_id())
               << ",\"id_string\":\"" << desc.id_string << "\""
               << ",\"display_name\":\"" << desc.display_name << "\""
               << ",\"state\":\"" << dev.get_state_string() << "\""
               << ",\"bus\":\"" << desc.physical_bus << "\""
               << ",\"voltage_spec\":\"" << desc.logic_voltage_level << "\""
               << ",\"clock\":\"" << desc.clock_frequency << "\""
               << ",\"capabilities\":\"" << desc.capabilities_desc << "\""
               << ",\"telemetry\":{"
               << "\"v_main\":" << telem.rail_voltage_main
               << ",\"v_aux\":" << telem.rail_voltage_aux
               << ",\"v_core\":" << telem.rail_voltage_core
               << ",\"current_mA\":" << telem.current_draw_mA
               << ",\"temp_C\":" << telem.die_temperature_C
               << ",\"uptime\":" << telem.uptime_ticks
               << ",\"packets_sent\":" << telem.packets_sent
               << ",\"packets_lost\":" << telem.packets_lost
               << ",\"recovered_faults\":" << telem.recovered_faults
               << "}}";
            if (++count < dev_map.size()) ss << ",";
        }
        ss << "],\"packet_history\":[";
        const auto& history = embedded_sys->get_packet_history();
        for (size_t i = 0; i < history.size(); ++i) {
            const auto& p = history[i];
            ss << "{\"seq\":" << p.sequence_num
               << ",\"device_id\":" << static_cast<int>(p.device_id)
               << ",\"device_name\":\"" << p.device_name << "\""
               << ",\"cmd_id\":" << static_cast<int>(p.cmd_id)
               << ",\"cmd_name\":\"" << p.cmd_name << "\""
               << ",\"state\":\"" << p.state_name << "\""
               << ",\"payload_len\":" << p.payload_len
               << ",\"payload_hex\":\"" << p.payload_hex << "\""
               << ",\"expected_crc\":" << p.expected_crc
               << ",\"received_crc\":" << p.received_crc
               << ",\"crc_valid\":" << (p.crc_valid ? "true" : "false")
               << ",\"is_retry\":" << (p.is_retry ? "true" : "false")
               << ",\"timestamp_ms\":" << p.timestamp_ms
               << ",\"status\":\"" << p.status_message << "\"}";
            if (i + 1 < history.size()) ss << ",";
        }
        ss << "]}";

        return HttpResponse::Json(ss.str());
    });

    server.Post("/api/embedded/action", [embedded_sys](const HttpRequest& req) -> HttpResponse {
        std::string action = req.get_param("action", "run_sequence");
        uint8_t dev_id = static_cast<uint8_t>(std::stoi(req.get_param("device_id", "16"))); // 0x10 = 16

        if (action == "run_sequence") {
            embedded_sys->execute_full_integration_sequence();
        } else if (action == "discover") {
            embedded_sys->send_device_command(dev_id, CommandId::CMD_DISCOVER);
        } else if (action == "query_caps") {
            embedded_sys->send_device_command(dev_id, CommandId::CMD_QUERY_CAPS);
        } else if (action == "connect") {
            embedded_sys->send_device_command(dev_id, CommandId::CMD_CONNECT);
        } else if (action == "telemetry") {
            bool fault = (req.get_param("inject_fault") == "true");
            embedded_sys->send_device_command(dev_id, CommandId::CMD_REQ_TELEMETRY, fault);
        } else if (action == "recover") {
            embedded_sys->send_device_command(dev_id, CommandId::CMD_FAULT_RECOVER);
        } else if (action == "disconnect") {
            embedded_sys->send_device_command(dev_id, CommandId::CMD_DISCONNECT);
        } else if (action == "reset") {
            embedded_sys->reset_all();
        }

        // Return updated state
        HttpRequest dummy;
        return HttpResponse::Json("{\"status\":\"ok\"}");
    });

    // 5. Backpropagation & Gradient Verification API
    server.Get("/api/backprop/train", [](const HttpRequest& req) -> HttpResponse {
        double lr = 0.1;
        int epochs = 100;
        try {
            if (!req.get_param("lr").empty()) lr = std::stod(req.get_param("lr"));
            if (!req.get_param("epochs").empty()) epochs = std::stoi(req.get_param("epochs"));
        } catch (...) {
            lr = 0.1;
            epochs = 100;
        }

        TrainingSimulationResult sim = run_backpropagation_simulation(1.0, 2.0, {0.5, 0.0, 1.0, 0.0}, lr, epochs);

        std::ostringstream ss;
        ss << "{\"learning_rate\":" << sim.learning_rate
           << ",\"total_epochs\":" << sim.total_epochs
           << ",\"all_grad_checks_passed\":" << (sim.all_grad_checks_passed ? "true" : "false")
           << ",\"gradient_check\":[";

        for (size_t i = 0; i < sim.initial_grad_check.size(); ++i) {
            const auto& g = sim.initial_grad_check[i];
            ss << "{\"param\":\"" << g.param_name << "\""
               << ",\"value\":" << g.param_val
               << ",\"analytical\":" << g.analytical_grad
               << ",\"numerical\":" << g.numerical_grad
               << ",\"abs_error\":" << g.abs_error
               << ",\"passed\":" << (g.passed ? "true" : "false") << "}";
            if (i + 1 < sim.initial_grad_check.size()) ss << ",";
        }

        ss << "],\"history\":[";
        for (size_t i = 0; i < sim.history.size(); ++i) {
            const auto& h = sim.history[i];
            ss << "{\"epoch\":" << h.epoch
               << ",\"w\":" << h.params.w
               << ",\"b\":" << h.params.b
               << ",\"v\":" << h.params.v
               << ",\"c\":" << h.params.c
               << ",\"z\":" << h.forward.z
               << ",\"h\":" << h.forward.h
               << ",\"y_hat\":" << h.forward.y_hat
               << ",\"loss\":" << h.forward.loss
               << ",\"dJ_dw\":" << h.grads.dJ_dw
               << ",\"dJ_db\":" << h.grads.dJ_db
               << ",\"dJ_dv\":" << h.grads.dJ_dv
               << ",\"dJ_dc\":" << h.grads.dJ_dc << "}";
            if (i + 1 < sim.history.size()) ss << ",";
        }
        ss << "]}";

        return HttpResponse::Json(ss.str());
    });

    // 6. CSV Download Routes
    server.Get("/api/export/fea.csv", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse::Csv(read_file_contents("results/raw_csv/fea_convergence.csv"), "fea_convergence.csv");
    });

    server.Get("/api/export/batch.csv", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse::Csv(read_file_contents("results/raw_csv/multicore_benchmark.csv"), "multicore_benchmark.csv");
    });

    server.Get("/api/export/backprop.csv", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse::Csv(read_file_contents("results/raw_csv/backprop_training.csv"), "backprop_training.csv");
    });

    // Start server
    if (!server.start()) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    std::cout << "[System Ready] Unified Workbench is running.\n";
    std::cout << "Open browser at: http://localhost:" << port << "\n";
    std::cout << "Press Ctrl+C to terminate server.\n" << std::endl;

    #ifdef _WIN32
    // Launch browser only after the HTTP server is 100% active and listening
    std::string url = "http://localhost:" + std::to_string(port);
    HINSTANCE res = ShellExecuteA(NULL, "open", "explorer.exe", url.c_str(), NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    #endif

    server.wait();
    return 0;
}
