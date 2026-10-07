#include "benchmark/simulator/simulator_benchmark.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "benchmark/common/histogram.hpp"
#include "benchmark/common/rdtsc.hpp"
#include "strategies/debug_strategy.hpp"

#ifndef HFT_SIMULATOR_BENCHMARK_CONFIG_PATH
#error "HFT_SIMULATOR_BENCHMARK_CONFIG_PATH must be supplied by CMake"
#endif

namespace {
    constexpr double BYTES_PER_GIB = 1024.0 * 1024.0 * 1024.0;
    constexpr auto CPU_WARMUP_DURATION = std::chrono::milliseconds(500);

    class EmptyInputSource final : public sim::IInputSource {
    public:
        bool read_chunk(const char *&, std::size_t &) override { return false; }
    };

    class MemoryInputSource final : public sim::IInputSource {
    public:
        explicit MemoryInputSource(const std::vector<char> &data) : data_(data) {}

        bool read_chunk(const char *&data, std::size_t &size) override {
            if (delivered_ || data_.empty()) return false;
            delivered_ = true;
            data = data_.data();
            size = data_.size();
            return true;
        }

    private:
        const std::vector<char> &data_;
        bool delivered_ = false;
    };

    std::unique_ptr<sim::DebugStrategy> make_strategy() {
        sim::DebugStrategy::Config config{};
        config.log_events = false;
        config.log_trades = false;
        return std::make_unique<sim::DebugStrategy>(config);
    }

    double warm_up_cpu() {
        const auto start = std::chrono::steady_clock::now();
        uint64_t state = 0x9e3779b97f4a7c15ULL;
        do {
            for (unsigned i = 0; i < 4096; ++i) {
                state ^= state << 7;
                state ^= state >> 9;
                state *= 0xbf58476d1ce4e5b9ULL;
            }
            benchmark::do_not_optimize(state);
        } while (std::chrono::steady_clock::now() - start <
                 CPU_WARMUP_DURATION);
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
    }

    double calibrate_tsc_hz() {
        using namespace std::chrono;
        constexpr auto duration = milliseconds(100);
        const auto wall_start = steady_clock::now();
        const auto tsc_start = benchmark::rdtsc_start();
        std::this_thread::sleep_for(duration);
        const auto tsc_end = benchmark::rdtsc_end();
        const auto wall_end = steady_clock::now();
        const double seconds = std::chrono::duration<double>(
            wall_end - wall_start).count();
        return seconds > 0.0 && tsc_end > tsc_start
                   ? static_cast<double>(tsc_end - tsc_start) / seconds
                   : 0.0;
    }

    double cycles_to_seconds(uint64_t cycles, double tsc_hz) {
        return tsc_hz > 0.0 ? static_cast<double>(cycles) / tsc_hz : 0.0;
    }

    benchmark::simulator::JournalDiagnostics diagnostics(
        const logging::TradeJournal::Stats &processing_end,
        const logging::TradeJournal::Stats &final) {
        return {
            .records_attempted = final.records_attempted,
            .records_accepted = final.records_accepted,
            .records_dropped = final.records_dropped,
            .records_written_at_processing_end = processing_end.records_written,
            .records_written = final.records_written,
            .queue_capacity = final.queue_capacity,
            .logger_failed = final.logger_failed
        };
    }

    bool diagnostics_valid(
        const benchmark::simulator::JournalDiagnostics &journal,
        bool input_complete) {
        return input_complete && !journal.logger_failed &&
               journal.records_dropped == 0 &&
               journal.records_accepted == journal.records_written;
    }

    std::filesystem::path stats_path_for(
        const std::filesystem::path &journal_path) {
        auto path = journal_path;
        path += ".stats.json";
        return path;
    }

    benchmark::simulator::JournalValidation validate_journal(
        const sim::Simulator::Config &config,
        const benchmark::simulator::JournalDiagnostics &expected) {
        benchmark::simulator::JournalValidation result;
        result.performed = true;
        const auto &path = config.trade_journal.path;
        const auto stats_path = stats_path_for(path);
        result.journal_file_exists = std::filesystem::is_regular_file(path);
        result.stats_file_exists = std::filesystem::is_regular_file(stats_path);

        if (config.trade_journal.mode ==
            logging::TradeJournal::Mode::Disabled) {
            result.first_sequence_valid = true;
            result.last_sequence_valid = true;
            result.persisted_stats_match = true;
            result.valid = !result.journal_file_exists &&
                           !result.stats_file_exists;
            if (!result.valid) {
                result.error = "disabled journal created output files";
            }
            return result;
        }

        if (!result.journal_file_exists || !result.stats_file_exists) {
            result.error = "journal or stats file is missing";
            return result;
        }

        try {
            std::ifstream journal(path);
            if (!journal) throw std::runtime_error("journal is unreadable");
            std::optional<uint64_t> first_sequence;
            std::optional<uint64_t> last_sequence;
            std::string line;
            while (std::getline(journal, line)) {
                if (line.empty()) continue;
                const auto record = nlohmann::json::parse(line)
                        .get<logging::TradeJournalRecord>();
                if (!first_sequence) first_sequence = record.sequence;
                last_sequence = record.sequence;
                ++result.file_records;
            }
            if (journal.bad()) throw std::runtime_error("journal read failed");

            result.first_sequence_valid = result.file_records == 0
                                              ? expected.records_written == 0
                                              : first_sequence == 0;
            result.last_sequence_valid = result.file_records == 0
                                             ? expected.records_written == 0
                                             : last_sequence == expected.records_written - 1;

            std::ifstream stats_file(stats_path);
            if (!stats_file) throw std::runtime_error("stats are unreadable");
            const auto stats = nlohmann::json::parse(stats_file);
            result.persisted_stats_match =
                    stats.at("records_attempted").get<uint64_t>() ==
                    expected.records_attempted &&
                    stats.at("records_accepted").get<uint64_t>() ==
                    expected.records_accepted &&
                    stats.at("records_dropped").get<uint64_t>() ==
                    expected.records_dropped &&
                    stats.at("records_written").get<uint64_t>() ==
                    expected.records_written &&
                    stats.at("queue_capacity").get<std::size_t>() ==
                    expected.queue_capacity &&
                    stats.at("logger_failed").get<bool>() == expected.logger_failed;

            result.valid = result.file_records == expected.records_written &&
                           expected.records_accepted == expected.records_written &&
                           expected.records_dropped == 0 &&
                           !expected.logger_failed &&
                           result.first_sequence_valid &&
                           result.last_sequence_valid &&
                           result.persisted_stats_match;
            if (!result.valid) result.error = "journal validation mismatch";
        } catch (const std::exception &error) {
            result.error = error.what();
        }
        return result;
    }

    template<typename Result>
    void populate_context(Result &result,
                          const sim::Simulator::Config &config,
                          std::size_t input_bytes) {
        result.config_path = config.source_path.string();
        result.effective_config = config;
        result.journal_path = config.trade_journal.path.string();
        result.input_bytes = input_bytes;
    }

    template<typename Result>
    void populate_journal_rates(Result &result) {
        // Every simulated trade is a journal candidate. Using that count keeps
        // the workload intensity comparable in Disabled mode and when an
        // asynchronous logger rejects records.
        if (result.processing_seconds > 0.0) {
            result.journal_candidates_per_second =
                    static_cast<double>(result.simulated_trades) /
                    result.processing_seconds;
        }
        if (result.frames > 0) {
            result.journal_candidate_percentage =
                    100.0 * static_cast<double>(result.simulated_trades) /
                    static_cast<double>(result.frames);
        }
    }
}

namespace benchmark::simulator {
    std::filesystem::path default_config_path() {
        return HFT_SIMULATOR_BENCHMARK_CONFIG_PATH;
    }

    SimulatorBenchmarkResult run_throughput(
        const std::vector<char> &data,
        const sim::Simulator::Config &config) {
        SimulatorBenchmarkResult result{};
        populate_context(result, config, data.size());

        auto strategy = make_strategy();
        auto *strategy_ptr = strategy.get();
        sim::Simulator simulator(std::make_unique<MemoryInputSource>(data),
                                 std::move(strategy), config);
        result.warmup_seconds = warm_up_cpu();

        sim::Simulator::RunMetrics metrics;
        {
            benchmark::ScopedPerfControl counters;
            simulator.run(metrics);
        }
        const auto final_stats = simulator.journal_stats();

        result.parsed = metrics.parsed;
        result.skipped = metrics.skipped;
        result.frames = result.parsed + result.skipped;
        result.input_complete = metrics.input_complete;
        result.simulated_trades = strategy_ptr->trades;
        result.checksum = strategy_ptr->checksum;
        result.journal = diagnostics(metrics.journal_at_processing_end, final_stats);
        result.processing_seconds = metrics.processing_seconds;
        result.finalization_seconds = metrics.finalization_seconds;
        result.total_seconds = metrics.total_seconds;
        if (result.processing_seconds > 0.0) {
            result.processing_frames_per_second =
                    static_cast<double>(result.frames) / result.processing_seconds;
            result.processing_gib_per_second =
                    (static_cast<double>(result.input_bytes) / BYTES_PER_GIB) /
                    result.processing_seconds;
        }
        if (result.total_seconds > 0.0) {
            result.total_frames_per_second =
                    static_cast<double>(result.frames) / result.total_seconds;
            result.total_gib_per_second =
                    (static_cast<double>(result.input_bytes) / BYTES_PER_GIB) /
                    result.total_seconds;
        }
        populate_journal_rates(result);
        result.journal_validation = validate_journal(config, result.journal);
        result.valid = diagnostics_valid(result.journal, result.input_complete) &&
                       result.journal_validation.valid;
        return result;
    }

    SimulatorLatencyBenchmarkResult run_latency(
        const std::vector<char> &data,
        const sim::Simulator::Config &config) {
        SimulatorLatencyBenchmarkResult result{};
        result.histogram.fill(0);
        populate_context(result, config, data.size());

        auto strategy = make_strategy();
        auto *strategy_ptr = strategy.get();
        sim::Simulator simulator(std::make_unique<EmptyInputSource>(),
                                 std::move(strategy), config);
        result.tsc_hz = calibrate_tsc_hz();
        result.warmup_seconds = warm_up_cpu();

        std::chrono::steady_clock::time_point processing_start, processing_end,
            finalization_start, finalization_end;
        logging::TradeJournal::Stats processing_stats;
        {
            benchmark::ScopedPerfControl counters;
            md::Cursor cursor(data.data(), data.size());
            processing_start = std::chrono::steady_clock::now();
            while (cursor.is_in_bounds(1)) {
                const auto start = benchmark::rdtsc_start();
                const auto parsed = simulator.process_one(cursor);
                const auto end = benchmark::rdtsc_end();
                if (parsed.needs_more_data) [[unlikely]] {
                    result.input_complete = false;
                    break;
                }
                ++result.frames;
                result.parsed += parsed.parsed;
                result.skipped += parsed.skipped;
                const auto cycles = end - start;
                const auto bucket = cycles < result.MAX_CYCLES
                                        ? cycles
                                        : result.MAX_CYCLES - 1;
                ++result.histogram[bucket];
            }
            processing_end = std::chrono::steady_clock::now();
            processing_stats = simulator.journal_stats();

            finalization_start = std::chrono::steady_clock::now();
            simulator.stop();
            finalization_end = std::chrono::steady_clock::now();
        }
        const auto final_stats = simulator.journal_stats();

        result.simulated_trades = strategy_ptr->trades;
        result.checksum = strategy_ptr->checksum;
        result.journal = diagnostics(processing_stats, final_stats);
        result.processing_seconds = std::chrono::duration<double>(
            processing_end - processing_start).count();
        result.finalization_seconds = std::chrono::duration<double>(
            finalization_end - finalization_start).count();
        result.total_seconds = std::chrono::duration<double>(
            finalization_end - processing_start).count();
        result.p50_cycles = benchmark::histogram_percentile(
            result.histogram, result.frames, 0.50);
        result.p99_cycles = benchmark::histogram_percentile(
            result.histogram, result.frames, 0.99);
        result.p999_cycles = benchmark::histogram_percentile(
            result.histogram, result.frames, 0.999);
        result.p50_seconds = cycles_to_seconds(result.p50_cycles, result.tsc_hz);
        result.p99_seconds = cycles_to_seconds(result.p99_cycles, result.tsc_hz);
        result.p999_seconds = cycles_to_seconds(result.p999_cycles, result.tsc_hz);
        populate_journal_rates(result);
        result.journal_validation = validate_journal(config, result.journal);
        result.valid = diagnostics_valid(result.journal, result.input_complete) &&
                       result.journal_validation.valid;
        return result;
    }

    void to_json(nlohmann::json &json, const JournalDiagnostics &v) {
        json = {
            {"records_attempted", v.records_attempted},
            {"records_accepted", v.records_accepted},
            {"records_dropped", v.records_dropped},
            {
                "records_written_at_processing_end",
                v.records_written_at_processing_end
            },
            {"records_written", v.records_written},
            {"queue_capacity", v.queue_capacity},
            {"logger_failed", v.logger_failed}
        };
    }

    void to_json(nlohmann::json &json, const JournalValidation &v) {
        json = {
            {"performed", v.performed}, {"valid", v.valid},
            {"journal_file_exists", v.journal_file_exists},
            {"stats_file_exists", v.stats_file_exists},
            {"file_records", v.file_records},
            {"first_sequence_valid", v.first_sequence_valid},
            {"last_sequence_valid", v.last_sequence_valid},
            {"persisted_stats_match", v.persisted_stats_match},
            {"error", v.error}
        };
    }

    void to_json(nlohmann::json &json, const SimulatorBenchmarkResult &r) {
        json = {
            {"measurement_scope", "simulator_run_including_finalization"},
            {"config_path", r.config_path},
            {"effective_config", r.effective_config},
            {"journal_path", r.journal_path}, {"input_bytes", r.input_bytes},
            {"parsed", r.parsed}, {"skipped", r.skipped},
            {"frames", r.frames}, {"simulated_trades", r.simulated_trades},
            {"checksum", r.checksum}, {"journal", r.journal},
            {"journal_validation", r.journal_validation},
            {"journal_retained", r.journal_retained},
            {"input_complete", r.input_complete}, {"valid", r.valid},
            {"warmup_seconds", r.warmup_seconds},
            {"processing_seconds", r.processing_seconds},
            {"finalization_seconds", r.finalization_seconds},
            {"total_seconds", r.total_seconds},
            {"processing_frames_per_second", r.processing_frames_per_second},
            {"total_frames_per_second", r.total_frames_per_second},
            {"processing_gib_per_second", r.processing_gib_per_second},
            {"total_gib_per_second", r.total_gib_per_second},
            {"journal_candidates_per_second", r.journal_candidates_per_second},
            {"journal_candidate_percentage", r.journal_candidate_percentage}
        };
    }

    void to_json(nlohmann::json &json,
                 const SimulatorLatencyBenchmarkResult &r) {
        json = {
            {"measurement_scope", "per_frame_process_one"},
            {"config_path", r.config_path},
            {"effective_config", r.effective_config},
            {"journal_path", r.journal_path}, {"input_bytes", r.input_bytes},
            {"parsed", r.parsed}, {"skipped", r.skipped},
            {"frames", r.frames}, {"simulated_trades", r.simulated_trades},
            {"checksum", r.checksum}, {"journal", r.journal},
            {"journal_validation", r.journal_validation},
            {"journal_retained", r.journal_retained},
            {"input_complete", r.input_complete}, {"valid", r.valid},
            {"p50_cycles", r.p50_cycles}, {"p99_cycles", r.p99_cycles},
            {"p999_cycles", r.p999_cycles}, {"tsc_hz", r.tsc_hz},
            {"p50_seconds", r.p50_seconds}, {"p99_seconds", r.p99_seconds},
            {"p999_seconds", r.p999_seconds},
            {"warmup_seconds", r.warmup_seconds},
            {"processing_seconds", r.processing_seconds},
            {"finalization_seconds", r.finalization_seconds},
            {"total_seconds", r.total_seconds},
            {"journal_candidates_per_second", r.journal_candidates_per_second},
            {"journal_candidate_percentage", r.journal_candidate_percentage}
        };
    }

    void print_result(const SimulatorBenchmarkResult &result) {
        std::cout << nlohmann::json(result).dump(4) << std::endl;
    }

    void print_result(const SimulatorLatencyBenchmarkResult &result,
                      const std::string &hist_path) {
        if (!hist_path.empty())
            benchmark::save_histogram(result.histogram,
                                      hist_path);
        std::cout << nlohmann::json(result).dump(4) << std::endl;
    }
} // namespace benchmark::simulator
