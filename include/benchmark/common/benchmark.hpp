//
// Created by silay on 5/21/26.
//

// Include all standard library headers BEFORE entering namespace benchmark
// to avoid std::numeric_limits collision with namespace benchmark
#include <iostream>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <x86intrin.h>
#include <random>
#include "benchmark/common//rdtsc.hpp"
#include "benchmark/common//perf_control.hpp"

#ifndef BENCHMARK_COMMON_BENCHMARK_HPP
#define BENCHMARK_COMMON_BENCHMARK_HPP

namespace benchmark {
    struct ThroughputResult {
        double warmup_seconds = 0.0;
        double seconds = 0.0;
        int error_code = 0;
        double operations_per_second = 0.0;
        size_t operations = 0;
    };

    struct LatencyResult {
        static constexpr size_t MAX_CYCLES = 1024 * 64;

        std::array<uint64_t, MAX_CYCLES> histogram{};
    };

    struct BenchmarkConfig {
        size_t runs = 1;
        uint32_t warmup_runs = 1000;
    };

    enum class BenchmarkMode : uint8_t {
        Throughput,
        Latency
    };

    void benchmark_warmup(size_t runs);

    template<typename Fn>
    ThroughputResult run_throughput(Fn &&fn) {
        std::cout << "Running throughput benchmark" << std::endl;
        auto time_start = std::chrono::steady_clock::now();
        ThroughputResult result;
        // WARMUP
        //benchmark_warmup(config.warmup_runs);

        PerfControl::enable();
        auto time_start_function = std::chrono::steady_clock::now();
        //result.operations = fn();
        fn();
        auto time_finish_function = std::chrono::steady_clock::now();
        PerfControl::disable();

        result.seconds = std::chrono::duration<double>(time_finish_function - time_start_function).count();
        // auto time_before = std::chrono::duration_cast<std::chrono::milliseconds>(benchmark_start - time_start);
        // auto time_after = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - benchmark_end);
        // double secondsBefore = std::chrono::duration<double>(time_before).count();
        // double secondsAfter = std::chrono::duration<double>(time_after).count();
        return result;
    }

    template<typename Fn>
    LatencyResult run_latency(Fn &&fn) {
        LatencyResult result;
        result.histogram.fill(0);

        while (true) {
            const uint64_t start = rdtsc_start();

            const bool finished = fn();

            const uint64_t end = rdtsc_end();

            if (finished) {
                break;
            }

            const uint64_t delta = end - start;

            if (delta < LatencyResult::MAX_CYCLES) {
                ++result.histogram[delta];
            } else {
                ++result.histogram[LatencyResult::MAX_CYCLES - 1];
            }
        }

        return result;
    }

    struct BenchmarkSessionConfig {
        size_t runs = 5;
        bool warmup_first_run = true;
        size_t warmup_iterations = 1000;
    };

    template<typename SetupFn, typename BenchmarkFn, typename ResultFn>
    void run_benchmark_session(const BenchmarkSessionConfig &config, SetupFn &&setup, BenchmarkFn &&logic,
                               ResultFn &&result_handler) {
        setup();

        if (config.warmup_first_run && config.runs > 0) {
            benchmark_warmup(config.warmup_iterations);
        }

        for (size_t run = 0; run < config.runs; ++run) {
            auto result = logic();
            result_handler(result);
        }
    }

    template<typename T>
    inline void do_not_optimize(T const &value) {
        asm volatile("" : : "g"(value) : "memory");
    }
}


#endif //BENCHMARK_COMMON_BENCHMARK_HPP
