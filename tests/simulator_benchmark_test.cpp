#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "benchmark/simulator/simulator_benchmark.hpp"
#include "simulator_test_data.hpp"

namespace {
    void require(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    std::filesystem::path write_config(std::string_view contents,
                                       std::string_view suffix) {
        const auto path = std::filesystem::temp_directory_path() /
                          ("hft_simulator_config_" + std::string(suffix) +
                           ".json");
        std::ofstream file(path, std::ios::out | std::ios::trunc);
        file << contents;
        return path;
    }

    std::string config_json(std::string_view mode,
                            std::string_view cpu = "null",
                            std::string_view logger_cpu = "null",
                            std::string_view throttle = "100") {
        return "{\"cpu_affinity\":" + std::string(cpu) +
               ",\"tracked_stocks\":[6556],\"event_throttle\":" +
               std::string(throttle) +
               ",\"trade_journal\":{\"path\":\"journal.jsonl\"," +
               "\"mode\":\"" + std::string(mode) +
               "\",\"logger_cpu\":" + std::string(logger_cpu) +
               ",\"idle_spin_count\":7}}";
    }

    void test_default_config() {
        const auto config = sim::load_simulator_config(
            benchmark::simulator::default_config_path());
        require(sim::trade_journal_mode_name(config.trade_journal.mode) !=
                    "unknown",
                "default config mode was not recognized");
        require(config.source_path.is_absolute(),
                "resolved config path must be absolute");
        require(config.tracked_stocks.size() <= md::ITCHParser::MaxTrackedStocks,
                "default config exceeds parser capacity");
        require(!config.event_throttle || *config.event_throttle > 0,
                "default event throttle is invalid");
    }

    void test_all_modes_and_relative_path() {
        using Mode = logging::TradeJournal::Mode;
        const std::pair<std::string_view, Mode> modes[]{
            {"disabled", Mode::Disabled}, {"on_stop", Mode::OnStop},
            {"real_time", Mode::RealTime},
            {"real_time_separate_thread", Mode::RealTimeSeparateThread}
        };
        for (const auto &[name, expected]: modes) {
            const auto path = write_config(config_json(name), name);
            const auto config = sim::load_simulator_config(path);
            require(config.trade_journal.mode == expected,
                    "parsed mode mismatch");
            require(config.trade_journal.idle_spin_count == 7,
                    "override spin count mismatch");
            require(config.event_throttle == 100,
                    "fixture event throttle mismatch");
            require(config.tracked_stocks == std::vector<dt::StockLocate>{6556},
                    "fixture tracked stocks mismatch");
            require(config.trade_journal.path ==
                    path.parent_path() / "journal.jsonl",
                    "relative journal path was not config-relative");
            std::filesystem::remove(path);
        }
    }

    void expect_invalid(std::string_view json, std::string_view suffix,
                        const char *message) {
        const auto path = write_config(json, suffix);
        bool threw = false;
        try {
            static_cast<void>(sim::load_simulator_config(path));
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        std::filesystem::remove(path);
        require(threw, message);
    }

    void test_invalid_config_is_rejected() {
        expect_invalid(config_json("unknown"), "unknown_mode",
                       "unknown logging mode was accepted");
        expect_invalid(
            R"({"cpu_affinity":null,"tracked_stocks":[],"event_throttle":100,"extra":1,"trade_journal":{"path":"j","mode":"disabled","logger_cpu":null,"idle_spin_count":1}})",
            "extra_field", "unknown config field was accepted");
        expect_invalid(config_json("disabled", "-1"), "negative_cpu",
                       "negative CPU was accepted");
        expect_invalid(config_json("disabled", "null", "null", "0"),
                       "zero_throttle", "zero throttle was accepted");
        expect_invalid(config_json("real_time_separate_thread", "8", "8"),
                       "same_cpu", "equal simulator/logger CPUs were accepted");
        auto json = nlohmann::json::parse(config_json("disabled"));
        json["tracked_stocks"] = nlohmann::json::array();
        for (std::size_t i = 0; i < md::ITCHParser::MaxTrackedStocks; ++i) {
            json["tracked_stocks"].push_back(i);
        }
        const auto path = write_config(json.dump(), "max_stocks");
        require(sim::load_simulator_config(path).tracked_stocks.size() == 16,
                "16 stocks were rejected");
        std::filesystem::remove(path);
        json["tracked_stocks"].push_back(16);
        expect_invalid(json.dump(), "too_many_stocks", "17 stocks were accepted");
        json["tracked_stocks"] = {1u, 1u};
        expect_invalid(json.dump(), "duplicates", "duplicate stocks were accepted");
    }

    class PerfCapture {
    public:
        PerfCapture() {
            path_ = std::filesystem::temp_directory_path() /
                    ("hft_perf_control_" + std::to_string(getpid()));
            require(mkfifo(path_.c_str(), 0600) == 0, "cannot create perf FIFO");
            fd_ = open(path_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
            require(fd_ >= 0, "cannot open perf FIFO");
            require(setenv("PERF_CTL_FIFO", path_.c_str(), 1) == 0,
                    "cannot set perf FIFO environment");
        }

        ~PerfCapture() {
            close(fd_);
            std::filesystem::remove(path_);
        }

        void expect_pairs(unsigned count) {
            char buffer[1024];
            const auto size = read(fd_, buffer, sizeof(buffer));
            std::string expected;
            for (unsigned i = 0; i < count; ++i) expected += "enable\ndisable\n";
            require(size > 0 && std::string(buffer, size) == expected,
                    "perf counters were not enabled/disabled around each workload");
        }

    private:
        std::filesystem::path path_;
        int fd_ = -1;
    };

    void test_trade_workload_all_modes() {
        auto data = simulator_test_data::add_order(7);
        const auto ask = simulator_test_data::add_order(7, 'S', 1001, 2);
        data.insert(data.end(), ask.begin(), ask.end());
        using Mode = logging::TradeJournal::Mode;
        uint64_t checksum = 0;
        for (const auto mode : {Mode::Disabled, Mode::OnStop, Mode::RealTime,
                                Mode::RealTimeSeparateThread}) {
            const auto path = std::filesystem::temp_directory_path() /
                              "hft_benchmark_modes.jsonl";
            const auto stats_path = path.string() + ".stats.json";
            std::filesystem::remove(path);
            std::filesystem::remove(stats_path);
            sim::Simulator::Config config{
                .tracked_stocks = {7}, .event_throttle = 1,
                .trade_journal = {.path = path, .mode = mode}};
            const auto result = benchmark::simulator::run_throughput(data, config);
            require(result.valid && result.parsed == 2 && result.frames == 2 &&
                    result.simulated_trades == 1, "trade workload mismatch");
            if (mode == Mode::Disabled) checksum = result.checksum;
            require(result.checksum == checksum, "logging changed strategy output");
            require(result.journal.records_written == (mode == Mode::Disabled ? 0 : 1),
                    "wrong persisted trade count");
            if (mode == Mode::OnStop) {
                require(result.journal.records_written_at_processing_end == 0,
                        "processing snapshot was taken after finalization");
            }
            require(result.total_seconds >= result.processing_seconds +
                    result.finalization_seconds, "total timing excludes a phase");
            require(result.journal_candidates_per_second > 0 &&
                    result.journal_candidate_percentage == 50.0,
                    "candidate rates must include disabled logging");
            const nlohmann::json json = result;
            require(json.contains("journal_candidates_per_second") &&
                    !json.contains("journal_records_per_second"),
                    "misleading journal rate name remains");
            const auto latency = benchmark::simulator::run_latency(data, config);
            require(latency.valid && latency.checksum == checksum &&
                    latency.simulated_trades == 1,
                    "per-frame and bulk simulator outcomes diverge");
            std::filesystem::remove(path);
            std::filesystem::remove(stats_path);
        }
    }

    void test_incomplete_empty_and_failed_runs() {
        sim::Simulator::Config config;
        const auto truncated = simulator_test_data::add_order(7);
        auto data = truncated;
        data.pop_back();
        const auto throughput = benchmark::simulator::run_throughput(data, config);
        require(!throughput.input_complete && !throughput.valid && throughput.frames == 0,
                "truncated bulk input was accepted");
        const auto latency = benchmark::simulator::run_latency(data, config);
        require(!latency.input_complete && !latency.valid && latency.frames == 0,
                "truncated latency input was accepted");
        const auto empty = benchmark::simulator::run_throughput({}, config);
        require(empty.valid && empty.frames == 0, "empty input was rejected");
        config.trade_journal.mode = logging::TradeJournal::Mode::OnStop;
        config.trade_journal.path = std::filesystem::temp_directory_path();
        bool threw = false;
        try {
            benchmark::simulator::run_throughput(truncated, config);
        } catch (const std::runtime_error &) {
            threw = true;
        }
        require(threw, "finalization failure was swallowed");
    }

    void test_latency_samples_every_completed_frame() {
        const std::vector<char> data{0, 0, 0, 1, 'S'};
        const auto journal_path = std::filesystem::temp_directory_path() /
                                  "hft_simulator_benchmark_disabled.jsonl";
        auto stats_path = journal_path;
        stats_path += ".stats.json";
        std::filesystem::remove(journal_path);
        std::filesystem::remove(stats_path);

        sim::Simulator::Config config{
            .cpu_affinity = std::nullopt,
            .tracked_stocks = {},
            .event_throttle = std::nullopt,
            .trade_journal = {
                .path = journal_path,
                .mode = logging::TradeJournal::Mode::Disabled,
                .idle_spin_count = 0,
                .logger_cpu = std::nullopt},
            .source_path = "test-config.json"
        };
        const auto throughput = benchmark::simulator::run_throughput(data,
                                                                      config);
        require(throughput.frames == 2 && throughput.skipped == 2,
                "throughput did not count every completed frame");
        require(throughput.valid, "disabled throughput result is invalid");

        const auto latency = benchmark::simulator::run_latency(data, config);
        const auto samples = std::accumulate(
            latency.histogram.begin(), latency.histogram.end(), uint64_t{0});
        require(latency.frames == 2 && latency.skipped == 2,
                "latency did not count every completed frame");
        require(samples == latency.frames,
                "latency histogram sample count does not match frames");
        require(latency.valid, "disabled latency result is invalid");
        require(!std::filesystem::exists(journal_path),
                "disabled benchmark created a journal file");
    }
}

int main() {
    PerfCapture perf;
    test_default_config();
    test_all_modes_and_relative_path();
    test_invalid_config_is_rejected();
    test_latency_samples_every_completed_frame();
    perf.expect_pairs(2);
    test_trade_workload_all_modes();
    perf.expect_pairs(8);
    test_incomplete_empty_and_failed_runs();
    perf.expect_pairs(4);
    return 0;
}
