cmake_minimum_required(VERSION 3.15)
project(UnifiedEngineeringWorkbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if (MSVC)
    add_compile_options(/O2 /W4 /EHsc /permissive-)
else()
    add_compile_options(-O3 -Wall -Wextra -pedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/backend)

# Core library sources
set(BENCH_CORE_SOURCES
    src/backend/fea_solver.cpp
    src/backend/multicore_batch.cpp
    src/backend/embedded_mock.cpp
    src/backend/backprop.cpp
    src/backend/cad_ansys_parser.cpp
)

# 1. Main Server Executables
add_executable(workbench_server
    src/backend/main.cpp
    ${BENCH_CORE_SOURCES}
)

add_executable(workbench_engine
    src/backend/main.cpp
    ${BENCH_CORE_SOURCES}
)

if (WIN32)
    target_link_libraries(workbench_server PRIVATE ws2_32 shell32)
    target_link_libraries(workbench_engine PRIVATE ws2_32 shell32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(workbench_server PRIVATE Threads::Threads)
    target_link_libraries(workbench_engine PRIVATE Threads::Threads)
endif()

# 2. Automated Test Suite Executable
add_executable(test_workbench
    tests/test_main.cpp
    ${BENCH_CORE_SOURCES}
)

if (WIN32)
    target_link_libraries(test_workbench PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(test_workbench PRIVATE Threads::Threads)
endif()

# Output message
message(STATUS "Configured Unified Engineering Workbench for Windows / Linux / macOS")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
