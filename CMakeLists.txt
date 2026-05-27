cmake_minimum_required(VERSION 3.20)

project(hft_simulator LANGUAGES CXX)

#
# C++
#

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# OPTIONS
#

option(PERF_BUILD "Enable perf-friendly build flags" OFF)
option(ENABLE_LTO "Enable link-time optimization" ON)
option(NATIVE_ARCH "Enable -march=native" ON)

#
# GLOBAL COMPILE FLAGS
#

add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -pipe
)

#
# BUILD TYPE FLAGS
#

set(CMAKE_CXX_FLAGS_DEBUG
        "-O0 -g3"
)

set(CMAKE_CXX_FLAGS_RELEASE
        "-O3 -DNDEBUG"
)

#
# PERF BUILD FLAGS
#

if (PERF_BUILD)
    add_compile_options(
            -g
            -fno-omit-frame-pointer
    )
endif ()

#
# CPU-SPECIFIC OPTIMIZATION
#

if (NATIVE_ARCH)
    add_compile_options(
            -march=native
    )
endif ()

#
# LINK-TIME OPTIMIZATION
#

include(CheckIPOSupported)

if (ENABLE_LTO)
    check_ipo_supported(RESULT ipo_supported)

    if (ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
endif ()

#
# INCLUDES
#

include_directories(include)

#
# MARKET DATA
#

add_library(market_data STATIC
        src/market_data/itch_parser.cpp
)

#
# ORDER BOOK
#

add_library(order_book STATIC
        src/order_book/order_book.cpp
)

target_link_libraries(order_book
        PUBLIC market_data
)

#
# BENCHMARK FRAMEWORK
#

add_library(benchmark_core STATIC
        src/benchmark/common/benchmark.cpp
)

target_link_libraries(benchmark_core
        PUBLIC
        order_book
)

#
# SIMULATOR
#

add_library(simulator STATIC
        src/simulator/simulator.cpp
)

target_link_libraries(simulator
        PUBLIC
        order_book
)

#
# APPS
#

add_executable(replay_itch
        apps/replay_itch.cpp
)

target_link_libraries(replay_itch
        PRIVATE
        simulator
)

#
# PARSER BENCHMARKS
#

add_library(parser_benchmark STATIC
        src/benchmark/parser/parser_benchmark.cpp
)

target_link_libraries(parser_benchmark
        PUBLIC
        benchmark_core
        market_data
)

add_executable(parser_throughput
        apps/benchmarks/parser_throughput.cpp
)

target_link_libraries(parser_throughput
        PRIVATE
        parser_benchmark
)

add_executable(parser_latency
        apps/benchmarks/parser_latency.cpp
)

target_link_libraries(parser_latency
        PRIVATE
        parser_benchmark
)

#
# ORDER BOOK BENCHMARKS
#

add_executable(book_benchmark
        apps/benchmarks/book_benchmark.cpp
        include/benchmark/order_book/order_book_benchmark.hpp
        src/benchmark/order_book/order_book_benchmark.cpp
)

target_link_libraries(book_benchmark
        PRIVATE
        parser_benchmark
)

#
# SIMULATOR BENCHMARKS
#

add_executable(simulator_benchmark
        apps/benchmarks/simulator_benchmark.cpp
        src/benchmark/simulator/simulator_benchmark.cpp
        include/benchmark/simulator/simulator_benchmark.hpp
)

target_link_libraries(simulator_benchmark
        PRIVATE
        benchmark_core
)

#
# EXTERNAL DEPENDENCIES
#

include(FetchContent)

FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(benchmark_core
        PUBLIC
        nlohmann_json::nlohmann_json
)