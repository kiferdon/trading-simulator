#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "logging/trade_journal.hpp"
#include "common/thread_affinity.hpp"

namespace {
    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    bool same_trade(const sim::SimulatedTrade &lhs, const sim::SimulatedTrade &rhs) {
        return lhs.timestamp == rhs.timestamp &&
               lhs.side == rhs.side &&
               lhs.price == rhs.price &&
               lhs.quantity == rhs.quantity &&
               lhs.stock_locate == rhs.stock_locate;
    }

    std::vector<logging::TradeJournalRecord> read_records(
        const std::filesystem::path &path) {
        std::ifstream file(path);
        require(file.good(), "journal file was not created");

        std::vector<logging::TradeJournalRecord> records;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                records.push_back(nlohmann::json::parse(line)
                                      .get<logging::TradeJournalRecord>());
            }
        }
        return records;
    }

    nlohmann::json read_json(const std::filesystem::path &path) {
        std::ifstream file(path);
        require(file.good(), "JSON file was not created");
        return nlohmann::json::parse(file);
    }

    bool wait_for_written(logging::TradeJournal &journal, uint64_t expected) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            if (journal.stats().records_written >= expected) {
                return true;
            }
            std::this_thread::yield();
        }
        return false;
    }

#if defined(__linux__)
    void require_sticky_persistence_failure(logging::TradeJournal &journal) {
        std::string first_error;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            std::string error;
            try {
                journal.stop();
            } catch (const std::runtime_error &failure) {
                error = failure.what();
            }
            require(!error.empty(), "buffered write failure was not reported by stop");
            if (attempt == 0) first_error = error;
            require(error == first_error, "persistence failure was not sticky");
        }
        require(journal.stats().logger_failed,
                "failed persistence must be visible in journal diagnostics");
    }

    void test_buffered_write_failures_are_reported() {
        const auto directory = std::filesystem::temp_directory_path() /
                ("hft_trade_journal_write_failure_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        try {
            const auto journal_path = directory / "journal_full.jsonl";
            std::filesystem::create_symlink("/dev/full", journal_path);
            {
                logging::TradeJournal journal(journal_path);
                journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});
                require_sticky_persistence_failure(journal);
                require(journal.stats().records_written == 0,
                        "failed buffered journal write was counted as persisted");
                require(!std::filesystem::exists(journal.stats_path()),
                        "failed journal write must not publish successful stats");
            }

            using Mode = logging::TradeJournal::Mode;
            for (const auto mode: {Mode::OnStop, Mode::RealTime,
                                   Mode::RealTimeSeparateThread}) {
                const auto path = directory /
                        ("stats_full_" + std::to_string(static_cast<unsigned>(mode)) +
                         ".jsonl");
                std::filesystem::create_symlink("/dev/full", path.string() + ".stats.json");
                logging::TradeJournal journal({.path = path, .mode = mode});
                journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});
                require_sticky_persistence_failure(journal);
                require(read_records(path).size() == 1 &&
                        journal.stats().records_written == 1,
                        "stats failure must preserve the completed journal write");
            }
        } catch (...) {
            std::filesystem::remove_all(directory);
            throw;
        }
        std::filesystem::remove_all(directory);
    }

    unsigned first_allowed_cpu() {
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        const int error = pthread_getaffinity_np(
            pthread_self(), sizeof(cpu_set), &cpu_set);
        require(error == 0, "failed to read the test thread affinity");

        for (unsigned cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &cpu_set)) {
                return cpu;
            }
        }
        throw std::runtime_error("test process has no allowed CPU");
    }

    void test_current_thread_affinity_uses_exact_cpu() {
        const unsigned cpu = first_allowed_cpu();
        bool exact_affinity = false;
        std::exception_ptr failure;

        {
            std::jthread thread([&] {
                try {
                    common::pin_current_thread(cpu);
                    cpu_set_t cpu_set;
                    CPU_ZERO(&cpu_set);
                    const int error = pthread_getaffinity_np(
                        pthread_self(), sizeof(cpu_set), &cpu_set);
                    if (error != 0) {
                        throw std::system_error(
                            error, std::generic_category(),
                            "failed to inspect pinned test thread");
                    }
                    exact_affinity = CPU_COUNT(&cpu_set) == 1 &&
                                     CPU_ISSET(cpu, &cpu_set);
                } catch (...) {
                    failure = std::current_exception();
                }
            });
        }

        if (failure) {
            std::rethrow_exception(failure);
        }
        require(exact_affinity, "thread was not pinned to exactly one CPU");
    }

    void test_invalid_logger_cpu_fails_during_startup() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_invalid_cpu_test.jsonl";
        std::filesystem::remove(path);

        bool threw = false;
        try {
            logging::TradeJournal journal({
                .path = path,
                .mode = logging::TradeJournal::Mode::RealTimeSeparateThread,
                .logger_cpu = CPU_SETSIZE
            });
        } catch (const std::system_error &) {
            threw = true;
        }

        require(threw, "invalid logger CPU did not fail during construction");
        std::filesystem::remove(path);
    }

    void test_non_realtime_modes_do_not_apply_logger_affinity() {
        constexpr logging::TradeJournal::Mode modes[]{
            logging::TradeJournal::Mode::OnStop,
            logging::TradeJournal::Mode::RealTime
        };
        std::size_t index = 0;
        for (const auto mode: modes) {
            const auto path = std::filesystem::temp_directory_path() /
                              ("hft_trade_journal_non_threaded_cpu_test_" +
                               std::to_string(index) + ".jsonl");
            auto stats_path = path;
            stats_path += ".stats.json";
            std::filesystem::remove(path);
            std::filesystem::remove(stats_path);

            logging::TradeJournal journal({
                .path = path,
                .mode = mode,
                .logger_cpu = CPU_SETSIZE
            });
            journal.stop();

            std::filesystem::remove(path);
            std::filesystem::remove(stats_path);
            ++index;
        }
    }
#endif

    void test_record_round_trip() {
        const logging::TradeJournalRecord expected{
            42, sim::SimulatedTrade{1000, sim::Side::Sell, 12'345, 200, 7}
        };

        const nlohmann::json json = expected;
        const auto actual = json.get<logging::TradeJournalRecord>();

        require(actual.sequence == expected.sequence, "sequence did not round-trip");
        require(same_trade(actual.trade, expected.trade), "trade did not round-trip");
    }

    void test_journal_file_and_sequences() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        logging::TradeJournal journal(path);
        journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});
        journal.add_trade(sim::SimulatedTrade{11, sim::Side::Sell, 100, 6, 1});

        require(!std::filesystem::exists(path),
                "on-stop journal must not persist before stop");
        journal.stop();

        const auto records = read_records(path);

        require(records.size() == 2, "journal record count mismatch");
        require(records[0].sequence == 0, "first generated sequence must be zero");
        require(records[1].sequence == 1, "second generated sequence must be one");
        require(records[0].trade.side == sim::Side::Buy, "first trade side mismatch");
        require(records[1].trade.side == sim::Side::Sell, "second trade side mismatch");

        const auto stats = read_json(stats_path);
        require(stats.at("records_attempted") == 2, "attempted record count mismatch");
        require(stats.at("records_accepted") == 2, "accepted record count mismatch");
        require(stats.at("records_dropped") == 0, "unexpected dropped records");
        require(stats.at("records_written") == 2, "written record count mismatch");

        const auto first_contents = read_json(stats_path);
        journal.stop();
        require(read_json(stats_path) == first_contents,
                "successful repeated stop must not rewrite stats");
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_synchronous_real_time_mode_persists_each_trade() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_sync_real_time_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        logging::TradeJournal journal({
            .path = path,
            .mode = logging::TradeJournal::Mode::RealTime
        });
        journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});

        require(journal.stats().records_written == 1,
                "synchronous journal did not finish writing in add_trade");
        auto records = read_records(path);
        require(records.size() == 1,
                "synchronous journal did not persist the first trade");

        journal.add_trade(sim::SimulatedTrade{11, sim::Side::Sell, 100, 6, 1});
        require(journal.stats().records_written == 2,
                "synchronous journal written count mismatch");
        records = read_records(path);
        require(records.size() == 2 && records[1].sequence == 1,
                "synchronous journal sequence mismatch");

        journal.stop();
        const auto stats = read_json(stats_path);
        require(stats.at("records_dropped") == 0,
                "synchronous journal dropped records");
        require(stats.at("records_written") == 2,
                "synchronous journal final count mismatch");

        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_separate_thread_mode_persists_each_trade() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_real_time_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        logging::TradeJournal journal({
            .path = path,
            .mode = logging::TradeJournal::Mode::RealTimeSeparateThread,
            .idle_spin_count = 0,
#if defined(__linux__)
            .logger_cpu = first_allowed_cpu()
#endif
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});

        require(wait_for_written(journal, 1),
                "real-time journal did not persist first trade in time");

        auto records = read_records(path);
        require(records.size() == 1, "real-time journal did not persist first trade");

        journal.add_trade(sim::SimulatedTrade{11, sim::Side::Sell, 100, 6, 1});

        require(wait_for_written(journal, 2),
                "real-time journal did not persist second trade in time");

        records = read_records(path);
        require(records.size() == 2, "real-time journal did not persist second trade");
        require(records[1].sequence == 1, "real-time sequence mismatch");
        require(!std::filesystem::exists(stats_path),
                "real-time stats must not be written before stop");

        journal.stop();
        const auto stats = read_json(stats_path);
        require(stats.at("records_dropped") == 0, "real-time journal dropped records");
        require(stats.at("records_written") == 2, "real-time written count mismatch");

        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_separate_thread_mode_drops_when_queue_is_full() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_overflow_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        logging::TradeJournal journal({
            .path = path,
            .mode = logging::TradeJournal::Mode::RealTimeSeparateThread
        });

        constexpr uint64_t attempted = 50'000;
        for (uint64_t i = 0; i < attempted; ++i) {
            journal.add_trade(sim::SimulatedTrade{i, sim::Side::Buy, 101, 1, 1});
        }
        journal.stop();

        const auto live_stats = journal.stats();
        require(live_stats.records_attempted == attempted, "overflow attempt count mismatch");
        require(live_stats.records_dropped > 0, "full queue did not drop records");
        require(live_stats.records_accepted + live_stats.records_dropped == attempted,
                "accepted and dropped counts do not cover all attempts");
        require(live_stats.records_written == live_stats.records_accepted,
                "accepted records were not fully drained");

        const auto persisted_stats = read_json(stats_path);
        require(persisted_stats.at("records_dropped") == live_stats.records_dropped,
                "persisted dropped count mismatch");
        require(persisted_stats.at("queue_capacity") == logging::TradeJournal::QUEUE_CAPACITY,
                "persisted queue capacity mismatch");

        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_disabled_journal_has_no_file_side_effect() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_trade_journal_disabled_test.jsonl";
        std::filesystem::remove(path);

        logging::TradeJournal journal({
            .path = path,
            .mode = logging::TradeJournal::Mode::Disabled
        });
        journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});
        journal.stop();

        require(!std::filesystem::exists(path),
                "disabled journal must not create an output file");
    }

    void test_defaults_are_disabled() {
        require(logging::TradeJournal::Config{}.mode ==
                    logging::TradeJournal::Mode::Disabled,
                "default journal config must be disabled");
    }

    void test_stop_failure_is_sticky() {
        logging::TradeJournal journal({
            .path = std::filesystem::temp_directory_path(),
            .mode = logging::TradeJournal::Mode::OnStop
        });

        std::string first_error;
        std::string second_error;
        try {
            journal.stop();
        } catch (const std::runtime_error &error) {
            first_error = error.what();
        }
        try {
            journal.stop();
        } catch (const std::runtime_error &error) {
            second_error = error.what();
        }

        require(!first_error.empty(), "first failed stop must report an error");
        require(second_error == first_error,
                "repeated failed stop must rethrow the original error");

        bool add_threw = false;
        try {
            journal.add_trade(sim::SimulatedTrade{10, sim::Side::Buy, 101, 5, 1});
        } catch (const std::runtime_error &) {
            add_threw = true;
        }
        require(add_threw, "failed stop must permanently reject new records");
    }
}

int main() {
#if defined(__linux__)
    test_buffered_write_failures_are_reported();
    test_current_thread_affinity_uses_exact_cpu();
    test_invalid_logger_cpu_fails_during_startup();
    test_non_realtime_modes_do_not_apply_logger_affinity();
#endif
    test_record_round_trip();
    test_journal_file_and_sequences();
    test_synchronous_real_time_mode_persists_each_trade();
    test_separate_thread_mode_persists_each_trade();
    test_separate_thread_mode_drops_when_queue_is_full();
    test_disabled_journal_has_no_file_side_effect();
    test_defaults_are_disabled();
    test_stop_failure_is_sticky();
    std::cout << "Trade journal JSON checks passed\n";
    return 0;
}
