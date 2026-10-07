#ifndef HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP
#define HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "benchmark/common/benchmark.hpp"
#include "simulator/simulator.hpp"

namespace benchmark::simulator {
    struct JournalDiagnostics {
        uint64_t records_attempted = 0;
        uint64_t records_accepted = 0;
        uint64_t records_dropped = 0;
        uint64_t records_written_at_processing_end = 0;
        uint64_t records_written = 0;
        std::size_t queue_capacity = 0;
        bool logger_failed = false;
    };

    struct JournalValidation {
        bool performed = false;
        bool valid = false;
        bool journal_file_exists = false;
        bool stats_file_exists = false;
        uint64_t file_records = 0;
        bool first_sequence_valid = false;
        bool last_sequence_valid = false;
        bool persisted_stats_match = false;
        std::string error;
    };

    struct SimulatorBenchmarkResult {
        std::string config_path;
        sim::Simulator::Config effective_config;
        std::string journal_path;
        std::size_t input_bytes = 0;
        std::size_t parsed = 0;
        std::size_t skipped = 0;
        std::size_t frames = 0;
        std::size_t simulated_trades = 0;
        uint64_t checksum = 0;
        JournalDiagnostics journal;
        JournalValidation journal_validation;
        bool journal_retained = false;
        bool input_complete = true;
        bool valid = false;
        double warmup_seconds = 0.0;
        double processing_seconds = 0.0;
        double finalization_seconds = 0.0;
        double total_seconds = 0.0;
        double processing_frames_per_second = 0.0;
        double total_frames_per_second = 0.0;
        double processing_gib_per_second = 0.0;
        double total_gib_per_second = 0.0;
        // Candidates / processing_seconds, including disabled/dropped records.
        double journal_candidates_per_second = 0.0;
        double journal_candidate_percentage = 0.0;
    };

    struct SimulatorLatencyBenchmarkResult {
        static constexpr std::size_t MAX_CYCLES =
                benchmark::LatencyResult::MAX_CYCLES;

        std::array<uint64_t, MAX_CYCLES> histogram{};
        std::string config_path;
        sim::Simulator::Config effective_config;
        std::string journal_path;
        std::size_t input_bytes = 0;
        std::size_t parsed = 0;
        std::size_t skipped = 0;
        std::size_t frames = 0;
        std::size_t simulated_trades = 0;
        uint64_t checksum = 0;
        JournalDiagnostics journal;
        JournalValidation journal_validation;
        bool journal_retained = false;
        bool input_complete = true;
        bool valid = false;
        uint64_t p50_cycles = 0;
        uint64_t p99_cycles = 0;
        uint64_t p999_cycles = 0;
        double tsc_hz = 0.0;
        double p50_seconds = 0.0;
        double p99_seconds = 0.0;
        double p999_seconds = 0.0;
        double warmup_seconds = 0.0;
        double processing_seconds = 0.0;
        double finalization_seconds = 0.0;
        double total_seconds = 0.0;
        // Candidates / processing_seconds, including disabled/dropped records.
        double journal_candidates_per_second = 0.0;
        double journal_candidate_percentage = 0.0;
    };

    std::filesystem::path default_config_path();

    SimulatorBenchmarkResult run_throughput(
        const std::vector<char> &data,
        const sim::Simulator::Config &config
    );

    SimulatorLatencyBenchmarkResult run_latency(
        const std::vector<char> &data,
        const sim::Simulator::Config &config
    );

    void print_result(const SimulatorBenchmarkResult &result);
    void print_result(const SimulatorLatencyBenchmarkResult &result,
                      const std::string &hist_path);
    void to_json(nlohmann::json &json, const JournalDiagnostics &diagnostics);
    void to_json(nlohmann::json &json, const JournalValidation &validation);
    void to_json(nlohmann::json &json, const SimulatorBenchmarkResult &result);
    void to_json(nlohmann::json &json,
                 const SimulatorLatencyBenchmarkResult &result);
} // namespace benchmark::simulator

#endif // HFT_SIMULATOR_SIMULATOR_BENCHMARK_HPP
