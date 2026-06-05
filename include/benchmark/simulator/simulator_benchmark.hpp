//
// Created by Claude on 2026-05-29.
//

#ifndef HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP
#define HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "benchmark/common/benchmark.hpp"
#include "simulator/simulator.hpp"
#include <nlohmann/json.hpp>

namespace benchmark {
    namespace simulator {
        struct SimulatorBenchmarkResult {
            double seconds = 0.0;
            double messages_per_second = 0.0;
            size_t messages = 0;
            size_t skipped = 0;
            size_t simulated_trades = 0;
            uint64_t checksum = 0;
        };

        struct SimulatorLatencyBenchmarkResult {
            static constexpr size_t MAX_CYCLES = benchmark::LatencyResult::MAX_CYCLES;

            std::array<uint64_t, MAX_CYCLES> histogram{};
            size_t messages = 0;
            size_t simulated_trades = 0;
            uint64_t checksum = 0;

            uint64_t p50_cycles = 0;
            uint64_t p99_cycles = 0;
            uint64_t p999_cycles = 0;
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
            SimulatorBenchmarkResult,
            messages,
            skipped,
            simulated_trades,
            checksum,
            seconds,
            messages_per_second
        )

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
            SimulatorLatencyBenchmarkResult,
            messages,
            simulated_trades,
            checksum,
            p50_cycles,
            p99_cycles,
            p999_cycles
        )

        struct NullStrategy : public sim::IStrategy {
            void on_event(const sim::MarketEvent &) override {
            }

            void on_trade(const sim::SimulatedTrade &) override {
            }
        };

        SimulatorBenchmarkResult run_throughput(
            const std::vector<char> &data
        );

        SimulatorLatencyBenchmarkResult run_latency(
            const std::vector<char> &data
        );

        void print_result(const SimulatorBenchmarkResult &result);

        void print_result(const SimulatorLatencyBenchmarkResult &result, const std::string &hist_path);
    }
} // namespace benchmark::simulator

#endif // HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP