#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/common/output.hpp"
#include "benchmark/simulator/simulator_benchmark.hpp"
#include "common/common.hpp"

namespace {
    std::optional<std::filesystem::path> parse_config_override(
        int argc, char **argv
    ) {
        if (argc == 4) return std::nullopt;
        if (argc == 6 && std::string_view(argv[4]) == "--config") {
            return std::filesystem::path(argv[5]);
        }
        throw std::invalid_argument(
            "usage: simulator_throughput <runs> <market-data> <output-jsonl> "
            "[--config <path>]");
    }

    std::filesystem::path journal_path_for(const std::filesystem::path &artifact_dir,
                                           std::size_t run) {
        return artifact_dir /
               ("simulator_throughput_trade_journal_run_" +
                std::to_string(run) + ".jsonl");
    }

    void remove_previous_outputs(const std::filesystem::path &journal_path) {
        std::error_code error;
        std::filesystem::remove(journal_path, error);
        auto stats_path = journal_path;
        stats_path += ".stats.json";
        std::filesystem::remove(stats_path, error);
    }

    void apply_retention(benchmark::simulator::SimulatorBenchmarkResult &result,
                         std::size_t run, std::size_t runs) {
        const bool enabled = result.effective_config.trade_journal.mode !=
                             logging::TradeJournal::Mode::Disabled;
        if (enabled && result.valid && run + 1 < runs) {
            remove_previous_outputs(result.effective_config.trade_journal.path);
        }
        result.journal_retained = enabled &&
            std::filesystem::exists(result.effective_config.trade_journal.path);
    }
}

int main(int argc, char **argv) {
    try {
        if (argc < 4) {
            throw std::invalid_argument(
                "usage: simulator_throughput <runs> <market-data> "
                "<output-jsonl> [--config <path>]");
        }

        const int runs = std::atoi(argv[1]);
        if (runs < 1) {
            throw std::invalid_argument("run count must be positive");
        }

        const std::string data_path = argv[2];
        const std::string json_path = argv[3];
        const auto override = parse_config_override(argc, argv);
        const auto config = sim::load_simulator_config(
            override.value_or(benchmark::simulator::default_config_path()));

        std::cout << "Simulator throughput benchmark\n"
                  << "Runs: " << runs << '\n'
                  << "Config: " << config.source_path << '\n'
                  << "Mode: "
                  << sim::trade_journal_mode_name(config.trade_journal.mode)
                  << std::endl;

        const std::vector<char> data = common::read_file(data_path);

        benchmark::BenchmarkOutput output(json_path);
        const auto artifact_dir = benchmark::create_artifact_directory(json_path);
        benchmark::BenchmarkSessionConfig session_config;
        session_config.runs = runs;
        session_config.warmup_first_run = false;
        std::size_t run_index = 0;
        bool all_valid = true;

        benchmark::run_benchmark_session(
            session_config,
            [] {},
            [&] {
                const auto journal_path = journal_path_for(artifact_dir, run_index);
                auto run_config = config;
                run_config.trade_journal.path = journal_path;
                return benchmark::simulator::run_throughput(data, run_config);
            },
            [&](auto &result) {
                all_valid = all_valid && result.valid;
                apply_retention(result, run_index,
                                static_cast<std::size_t>(runs));
                benchmark::simulator::print_result(result);
                benchmark::write_json_result(result, output.json());
                ++run_index;
            }
        );
        if (!all_valid) {
            std::cerr << "Simulator throughput benchmark contains invalid runs\n";
            return 1;
        }
    } catch (const std::exception &error) {
        std::cerr << "Simulator throughput benchmark failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
