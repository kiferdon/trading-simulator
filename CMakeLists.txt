cmake_minimum_required(VERSION 3.20)

project(hft_simulator LANGUAGES CXX)

#
# C++
#

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
        src/order_book/order_book_v2_page_table.cpp
        src/order_book/order_book_v2_vector.cpp
        src/order_book/order_book_v2_hybrid.cpp
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

# INPUT SOURCE
#

add_library(input STATIC
        src/input/input_source.cpp
)

#
# SIMULATOR
#

add_library(simulator STATIC
        src/simulator/simulator.cpp
        src/simulator/market.cpp
        include/strategies/debug_strategy.hpp
        include/strategies/null_strategy.hpp
        include/strategies/trading_strategy.hpp
)

target_link_libraries(simulator
        PUBLIC
        order_book
        input
        nlohmann_json::nlohmann_json
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

add_library(order_book_benchmark STATIC
        src/benchmark/order_book/order_book_benchmark.cpp
)

target_link_libraries(order_book_benchmark
        PUBLIC
        market_data
        order_book
        parser_benchmark
)

add_executable(book_throughput
        apps/benchmarks/book_throughput.cpp
)

target_link_libraries(book_throughput
        PRIVATE
        order_book_benchmark
)

add_executable(book_latency
        apps/benchmarks/book_latency.cpp
)

target_link_libraries(book_latency
        PRIVATE
        order_book_benchmark
)

#
# ORDER BOOK MICROBENCHMARKS
#

add_library(order_book_microbenchmark STATIC
        src/benchmark/order_book/order_book_microbenchmark.cpp
)

target_link_libraries(order_book_microbenchmark
        PUBLIC
        order_book
        benchmark_core
)

add_executable(book_microbenchmark_throughput
        apps/benchmarks/book_microbenchmark_throughput.cpp
)

target_link_libraries(book_microbenchmark_throughput
        PRIVATE
        order_book_microbenchmark
)

add_executable(book_microbenchmark_latency
        apps/benchmarks/book_microbenchmark_latency.cpp
)

target_link_libraries(book_microbenchmark_latency
        PRIVATE
        order_book_microbenchmark
)

# SIMULATOR BENCHMARKS

add_library(simulator_benchmark STATIC
        src/benchmark/simulator/simulator_benchmark.cpp
)

target_link_libraries(simulator_benchmark
        PUBLIC
        benchmark_core
        simulator
        market_data
        order_book
)

add_executable(simulator_throughput
        apps/benchmarks/simulator_throughput.cpp
)

target_link_libraries(simulator_throughput
        PRIVATE
        simulator_benchmark
)

add_executable(simulator_latency
        apps/benchmarks/simulator_latency.cpp
)

target_link_libraries(simulator_latency
        PRIVATE
        simulator_benchmark
)

#
# TESTS
#

add_executable(itch_parser_test
        tests/itch_parser_test.cpp
)

target_link_libraries(itch_parser_test
        PRIVATE
        market_data
)

add_executable(order_book_test
        tests/order_book_test.cpp
)

target_link_libraries(order_book_test
        PRIVATE
        order_book
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
