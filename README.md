# PASS Science Works R&D Internship 2026–2027
# Unified Engineering Workbench

**Candidate Name:** KAVIN K K  
**Registration Number:** RA2311003020070  
**Role:** Principal Systems & Scientific Software Engineer  
**GitHub Repository:** [https://github.com/Kavinbuck77/RA2311003020070_Workbench](https://github.com/Kavinbuck77/RA2311003020070_Workbench)  
**Target Environment:** Windows 11 x64 (Clean run, no npm/node required, no administrative privileges required)  
**Standard:** Strictly ISO C++17 (`-std=c++17 -O3 -pthread`)  

---

## 1. System Overview

The **Unified Engineering Workbench** is a high-performance native scientific computing suite and hardware-in-the-loop dashboard. It couples native C++17 finite element analysis, parallel multicore batch processing, embedded hardware protocol state machines, and neural surrogate backpropagation with a responsive WebGL/Plotly dashboard served via an embedded Winsock2 HTTP server.

```
CandidateID_Workbench/
├── Launch.bat                   # 1-Click Launch script (Compiles if needed, starts engine, opens browser)
├── CMakeLists.txt               # Cross-platform CMake 3.15+ configuration
├── README.md                    # System architecture & execution manual
├── workbench_server.exe         # Compiled native C++17 server binary
├── test_runner.exe              # Compiled automated test suite
├── src/
│   ├── backend/
│   │   ├── fea_solver.hpp & .cpp        # Task C: 1D Steady-State Thermal FEA & O(h^2) Convergence
│   │   ├── multicore_batch.hpp & .cpp   # Task D: Chunked Parallel Executor & Benchmark
│   │   ├── embedded_mock.hpp & .cpp     # Task E: STM32, Artix-7, FT2232H HIL State Machines & Fault Recovery
│   │   ├── backprop.hpp & .cpp          # Task F: Symbolic Reverse-Mode Autodiff & Gradient Check
│   │   ├── cad_ansys_parser.hpp & .cpp  # Task B: ASCII STL & ANSYS CSV State Parsers
│   │   ├── http_server.hpp              # Native Winsock2 Multi-threaded HTTP/1.1 REST Server
│   │   └── main.cpp                     # Application entrypoint & REST routing
│   └── frontend/
│       └── index.html                   # Mission Control Dashboard (Three.js WebGL & Plotly.js)
├── data/
│   ├── sample.stl                       # 3D Mechanical Heat Sink CAD Geometry (ASCII STL)
│   └── ansys_nodes.csv                  # 288 3D Thermal Nodal Temperature Field
├── tests/
│   └── test_main.cpp                    # Comprehensive Automated Numerical & Unit Test Suite
├── results/
│   ├── raw_csv/
│   │   ├── fea_convergence.csv          # Exported FEA mesh convergence dataset
│   │   ├── multicore_benchmark.csv      # Exported 1000-solve multithreaded scaling data
│   │   └── backprop_training.csv        # Exported 100-epoch neural training trajectory
│   └── plots/                           # Exported plot logs and assets
└── docs/
    ├── REPORT.md                        # Complete 6-Page Technical Engineering Report
    ├── REPORT_OUTLINE.md                # Report structure & outline
    ├── HANDWRITTEN_DERIVATION.md        # 2-Page Step-by-Step LaTeX Backprop Derivation Scan Guide
    └── ai_log.md                        # AI execution & verification log
```

---

## 2. Quick Start (Windows 11)

### Option A: One-Click Launch (Recommended)
Double-click `Launch.bat` or execute in PowerShell/Command Prompt:
```cmd
.\Launch.bat
```
*This automatically checks the native engine binary, compiles it if needed, launches the background engine, and opens your default browser directly to `http://localhost:8080`.*

### Option B: Direct Compilation & Run
```cmd
g++ -std=c++17 -O3 src/backend/main.cpp src/backend/fea_solver.cpp src/backend/multicore_batch.cpp src/backend/embedded_mock.cpp src/backend/backprop.cpp src/backend/cad_ansys_parser.cpp -lws2_32 -o workbench_server.exe
.\workbench_server.exe 8080
```
Then navigate to: `http://localhost:8080`

### Option C: CMake Build
```cmd
cmake -B build -G "MinGW Makefiles"
cmake --build build --config Release
.\build\workbench_server.exe
```

---

## 3. Running the Automated Test Suite

To run the complete automated test suite verifying 100% numerical accuracy and edge case validation:
```cmd
g++ -std=c++17 -O3 tests/test_main.cpp src/backend/fea_solver.cpp src/backend/multicore_batch.cpp src/backend/embedded_mock.cpp src/backend/backprop.cpp src/backend/cad_ansys_parser.cpp -o test_runner.exe
.\test_runner.exe
```

#### Test Suite Verification Output:
- **Task C (FEA):** Verified exact nodal superconvergence ($|T_{\text{fea}} - T_{\text{exact}}| < 10^{-14}\text{ C}$) and exact second-order midpoint convergence rate $\alpha = 2.0000$ ($O(h^2)$) across $N = 10, 20, 40, 80$.
- **Task D (Multicore):** 1000 solves across 1, 2, and 4 threads verified to match the serial solution with max absolute discrepancy $= 0.00 \times 10^0 \le 10^{-8}$.
- **Task E (Embedded):** Verified state machines for STM32, Artix-7, and FT2232H, with transient CRC-16 parity fault injection on Command #4 and recovery.
- **Task F (Backprop):** Finite difference gradient checks with $\epsilon = 10^{-6}$ verified analytical chain-rule gradients within error $\le 1.13 \times 10^{-10} \ll 10^{-5}$ (PASS). Training loss converges from $1.1825 \to 1.70 \times 10^{-20}$.
- **Task B (Parsers):** Successfully parsed valid 3D STL & 5-column ANSYS CSV files, and rejected malformed inputs (NaN floats, incomplete loops, duplicate node IDs).

---

## 4. REST API Documentation

The native engine exposes the following HTTP/1.1 REST endpoints on port 8080:

| Method | Endpoint | Query Parameters | Description |
| :--- | :--- | :--- | :--- |
| `GET` | `/` | — | Serves the single-page responsive dashboard (`index.html`) |
| `GET` | `/api/cad/stl` | — | Returns JSON parsed STL vertices, normals, and bounding box |
| `GET` | `/api/cad/ansys` | — | Returns JSON parsed ANSYS 3D points and temperature field |
| `GET` | `/api/fea/solve` | `L, A, k, q, T0, TL` | Solves 1D FEA for $N=10, 20, 40, 80$, returns convergence metrics |
| `GET` | `/api/batch/benchmark`| `cases, seed` | Executes multicore scaling benchmark across 1, 2, 4 threads |
| `GET` | `/api/embedded/state` | — | Returns current state, telemetry, and packet history for all 3 devices |
| `POST`| `/api/embedded/action`| `action, device_id, inject_fault` | Sends command (discover, connect, telemetry, fault_inject, recover) |
| `GET` | `/api/backprop/train` | `lr, epochs` | Runs finite difference gradient check and 100-epoch training simulation |
| `GET` | `/api/export/fea.csv` | — | Direct CSV download of FEA convergence table |
| `GET` | `/api/export/batch.csv`| — | Direct CSV download of multicore scaling benchmark |
| `GET` | `/api/export/backprop.csv`| — | Direct CSV download of neural training trajectory |

---

## 5. Technical Documentation Links
- [Complete Technical Report (docs/REPORT.md)](docs/REPORT.md)
- [Report Outline (docs/REPORT_OUTLINE.md)](docs/REPORT_OUTLINE.md)
- [2-Page Step-by-Step Handwritten Derivation Guide (docs/HANDWRITTEN_DERIVATION.md)](docs/HANDWRITTEN_DERIVATION.md)
- [AI Execution & Verification Log (docs/ai_log.md)](docs/ai_log.md)
