//
// Created by Claude on 2026-05-29.
//

#include "benchmark/simulator/simulator_benchmark.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

#include "benchmark/common/histogram.hpp"
#include "benchmark/common/rdtsc.hpp"
#include "common/common.hpp"
#include "market_data/itch_parser.hpp"
#include "simulator/simulator.hpp"
#include "strategies/debug_strategy.hpp"

namespace {
    class MemoryInputSource : public sim::IInputSource {
    public:
        explicit MemoryInputSource(const std::vector<char> &d) : data(d), pos(0) {
        }

        bool read_chunk(const char *&d, size_t &s) override {
            if (pos >= data.size()) return false;
            d = data.data() + pos;
            s = data.size() - pos;
            pos = data.size();
            return true;
        }

    private:
        const std::vector<char> &data;
        size_t pos = 0;
    };

    class EmptyInputSource : public sim::IInputSource {
    public:
        bool read_chunk(const char *&, size_t &) override {
            return false;
        }
    };

    double calibrate_tsc_hz() {
        using namespace std::chrono;

        constexpr auto calibration_duration = milliseconds(100);
        const auto wall_start = steady_clock::now();
        const uint64_t tsc_start = benchmark::rdtsc_start();

        std::this_thread::sleep_for(calibration_duration);

        const uint64_t tsc_end = benchmark::rdtsc_end();
        const auto wall_end = steady_clock::now();

        const double seconds = duration<double>(wall_end - wall_start).count();
        if (seconds <= 0.0 || tsc_end <= tsc_start) {
            return 0.0;
        }

        return static_cast<double>(tsc_end - tsc_start) / seconds;
    }

    double cycles_to_seconds(uint64_t cycles, double tsc_hz) {
        if (tsc_hz <= 0.0) {
            return 0.0;
        }
        return static_cast<double>(cycles) / tsc_hz;
    }
}

namespace benchmark {
    namespace simulator {
        SimulatorBenchmarkResult run_throughput(
            const std::vector<char> &data
        ) {
            SimulatorBenchmarkResult bench_result{};

            auto result = benchmark::run_throughput([&] {
                sim::DebugStrategy::Config config{};

                config.flush_mode = sim::DebugStrategy::FlushMode::OnFinish;
                config.log_events = false;
                config.log_file_path = "sim_throughput_strategy.log";
                config.log_trades = true;

                std::unique_ptr<sim::DebugStrategy> strategy = std::make_unique<sim::DebugStrategy>(config);
                sim::DebugStrategy *strategy_ptr = strategy.get();

                sim::Simulator sim(
                    std::make_unique<MemoryInputSource>(data),
                    std::move(strategy)
                );


                sim.set_strategy_simulator();
                sim.add_tracked_stock(md::STOCK_LOCATE_QQQ);
                sim.set_event_throttle(std::optional<dt::StockLocate>(md::STOCK_LOCATE_QQQ), 100);
                sim.run();

                bench_result.simulated_trades = strategy_ptr->trades;
                bench_result.checksum = strategy_ptr->checksum;
                bench_result.messages = data.size();
            });

            bench_result.seconds = result.seconds;
            bench_result.messages_per_second = data.size() / result.seconds;

            return bench_result;
        }

        SimulatorLatencyBenchmarkResult run_latency(
            const std::vector<char> &data
        ) {
            SimulatorLatencyBenchmarkResult result{};
            result.histogram.fill(0);

            sim::DebugStrategy::Config config{};

            config.flush_mode = sim::DebugStrategy::FlushMode::OnFinish;
            config.log_events = false;
            config.log_file_path = "sim_latency_strategy.log";
            config.log_trades = true;

            std::unique_ptr<sim::DebugStrategy> strategy = std::make_unique<sim::DebugStrategy>(config);
            sim::DebugStrategy *strategy_ptr = strategy.get();

            sim::Simulator sim(
                std::make_unique<EmptyInputSource>(),
                std::move(strategy)
            );
            sim.set_strategy_simulator();
            sim.add_tracked_stock(md::STOCK_LOCATE_QQQ);
            sim.set_event_throttle(std::optional<dt::StockLocate>(md::STOCK_LOCATE_QQQ), 100);

            md::Cursor cursor(data.data(), data.size());

            while (cursor.is_in_bounds(1)) {
                const uint64_t start = benchmark::rdtsc_start();
                md::ParseOneResult parse_result = sim.process_one(cursor);
                const uint64_t end = benchmark::rdtsc_end();

                if (parse_result.needs_more_data) [[unlikely]] {
                    std::cerr << "Need more data, breaking simulator latency benchmark\n";
                    break;
                }

                if (parse_result.parsed) {
                    ++result.messages;
                    const uint64_t cycles = end - start;
                    const uint64_t bucket = cycles < SimulatorLatencyBenchmarkResult::MAX_CYCLES
                                                ? cycles
                                                : SimulatorLatencyBenchmarkResult::MAX_CYCLES - 1;
                    ++result.histogram[bucket];
                } else if (parse_result.skipped) {
                    ++result.skipped;
                }
            }

            result.simulated_trades = strategy_ptr->trades;
            result.checksum = strategy_ptr->checksum;
            result.p50_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.50);
            result.p99_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.99);
            result.p999_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.999);
            result.tsc_hz = calibrate_tsc_hz();
            result.p50_seconds = cycles_to_seconds(result.p50_cycles, result.tsc_hz);
            result.p99_seconds = cycles_to_seconds(result.p99_cycles, result.tsc_hz);
            result.p999_seconds = cycles_to_seconds(result.p999_cycles, result.tsc_hz);

            return result;
        }

        void print_result(const SimulatorBenchmarkResult &result) {
            nlohmann::json j = result;
            std::cout << j.dump(4) << std::endl;
        }

        void print_result(
            const SimulatorLatencyBenchmarkResult &result,
            const std::string &hist_path
        ) {
            nlohmann::json j = result;

            if (!hist_path.empty()) {
                benchmark::save_histogram(result.histogram, hist_path);
            }

            std::cout << j.dump(4) << std::endl;
        }
    }
} // namespace benchmark::simulator
