#ifndef HFT_SIMULATOR_TRADE_JOURNAL_HPP
#define HFT_SIMULATOR_TRADE_JOURNAL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "benchmark/common/spsc_queue.hpp"
#include "simulator/strategy.hpp"

namespace logging {
    struct TradeJournalRecord {
        uint64_t sequence = 0;
        sim::SimulatedTrade trade{};
    };

    class TradeJournal {
    public:
        static std::string DEFAULT_PATH;
        static constexpr std::size_t QUEUE_CAPACITY = 1024;

        enum class Mode : uint8_t {
            Disabled,
            OnStop,
            RealTime,
            RealTimeSeparateThread
        };

        struct Config {
            std::filesystem::path path = DEFAULT_PATH;
            Mode mode = Mode::Disabled;
            std::size_t idle_spin_count = 1024;
            // RealTimeSeparateThread pins the logger before construction
            // completes. Other modes do not create a logger thread.
            std::optional<unsigned> logger_cpu = std::nullopt;
        };

        struct Stats {
            // In RealTimeSeparateThread mode, attempted = accepted + dropped.
            // A full queue rejects the new record without blocking the producer.
            uint64_t records_attempted = 0;
            uint64_t records_accepted = 0;
            uint64_t records_dropped = 0;
            uint64_t records_written = 0;
            std::size_t queue_capacity = QUEUE_CAPACITY;
            // Includes journal and stats persistence errors reported by stop().
            bool logger_failed = false;
        };

        TradeJournal();

        explicit TradeJournal(std::filesystem::path path);

        explicit TradeJournal(Config config);

        ~TradeJournal();

        void add_record(const TradeJournalRecord &record);

        void add_trade(const sim::SimulatedTrade &trade);

        // The producer thread owns add_record(), add_trade(), and stop().
        // A successful repeated stop is a no-op. A failed stop retains and
        // rethrows the original persistence error on subsequent calls.
        void stop();

        // Safe on the producer thread while running, or from any thread after
        // stop(). Concurrent observation from another thread is unsupported.
        [[nodiscard]] Stats stats() const noexcept;

        [[nodiscard]] const std::filesystem::path &stats_path() const noexcept;

    private:
        enum class LifecycleState {
            Running,
            Stopped,
            Failed
        };

        enum class LoggerStartupState : uint8_t {
            Starting,
            Ready,
            Failed
        };

        void log_loop();

        void append_record(const TradeJournalRecord &record);

        void persist_buffered();

        void finish_real_time();

        void finish_synchronous_real_time();

        void notify_logger() noexcept;

        void write_stats() const;

        bool try_accept(const TradeJournalRecord &record);

        Config config_;
        std::filesystem::path stats_path_;
        std::ofstream real_time_file_;
        std::vector<TradeJournalRecord> records_;
        uint64_t next_sequence_ = 0;
        SpscQueue<TradeJournalRecord, QUEUE_CAPACITY> queue_;
        std::atomic<bool> producer_done_{false};
        std::atomic<uint64_t> work_generation_{0};
        std::atomic<bool> logger_failed_{false};
        std::atomic<LoggerStartupState> logger_startup_state_{
            LoggerStartupState::Starting
        };
        std::exception_ptr logger_exception_;
        std::exception_ptr terminal_exception_;
        std::jthread logger_thread_;
        LifecycleState lifecycle_state_ = LifecycleState::Running;
        uint64_t accepted_records_ = 0;
        uint64_t dropped_records_ = 0;
        std::atomic<uint64_t> written_records_{0};
    };
} // namespace logging

namespace nlohmann {
    template<>
    struct adl_serializer<logging::TradeJournalRecord> {
        static void to_json(json &j, const logging::TradeJournalRecord &record) {
            j = json{
                {"sequence_id", record.sequence},
                {"trade", record.trade}
            };
        }

        static void from_json(const json &j, logging::TradeJournalRecord &record) {
            record.sequence = j.at("sequence_id").get<uint64_t>();
            record.trade = j.at("trade").get<sim::SimulatedTrade>();
        }
    };
} // namespace nlohmann

#endif // HFT_SIMULATOR_TRADE_JOURNAL_HPP
