//
// Created by Claude on 2026-05-29.
//

#include "benchmark/simulator/simulator_benchmark.hpp"

#include <iostream>
#include <memory>
#include <optional>

#include "benchmark/common/histogram.hpp"
#include "common/common.hpp"
#include "market_data/itch_parser.hpp"
#include "simulator/simulator.hpp"

namespace {
    class MemoryInputSource : public sim::IInputSource {
    public:
        explicit MemoryInputSource(const std::vector<char> &d) : data(d), pos(0) {}

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
}

namespace benchmark {
    namespace simulator {
        SimulatorBenchmarkResult run_throughput(
            const std::vector<char> &data
        ) {
            auto result = benchmark::run_throughput([&] {
                sim::Simulator sim(
                    std::make_unique<MemoryInputSource>(data),
                    std::make_unique<NullStrategy>()
                );
                sim.add_tracked_stock(md::STOCK_LOCATE_QQQ);
                sim.set_event_throttle(std::optional<dt::StockLocate>(md::STOCK_LOCATE_QQQ), 100);
                sim.run();
            });

            SimulatorBenchmarkResult bench_result{};
            bench_result.seconds = result.seconds;
            bench_result.messages_per_second = data.size() / result.seconds;
            bench_result.messages = data.size();
            bench_result.simulated_trades = 0;

            return bench_result;
        }

        SimulatorLatencyBenchmarkResult run_latency(
            const std::vector<char> &data
        ) {
            SimulatorLatencyBenchmarkResult result{};

            auto benchmark_result = benchmark::run_latency([&]() -> bool {
                sim::Simulator sim(
                    std::make_unique<MemoryInputSource>(data),
                    std::make_unique<NullStrategy>()
                );
                sim.add_tracked_stock(md::STOCK_LOCATE_QQQ);
                sim.add_tracked_stock(md::STOCK_LOCATE_SPY);
                sim.set_event_throttle(std::nullopt, 100);
                sim.run();
                return true;
            });

            result.histogram = benchmark_result.histogram;

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

            j["p50_cycles"] = benchmark::histogram_percentile(
                result.histogram, result.messages, 0.50);
            j["p99_cycles"] = benchmark::histogram_percentile(
                result.histogram, result.messages, 0.99);
            j["p999_cycles"] = benchmark::histogram_percentile(
                result.histogram, result.messages, 0.999);

            benchmark::save_histogram(result.histogram, hist_path);

            std::cout << j.dump(4) << std::endl;
        }
    }
} // namespace benchmark::simulator