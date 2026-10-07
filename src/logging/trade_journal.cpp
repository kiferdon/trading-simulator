#include "logging/trade_journal.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "common/thread_affinity.hpp"

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

namespace {
    void cpu_relax() noexcept {
#if defined(__i386__) || defined(__x86_64__)
        _mm_pause();
#else
        std::this_thread::yield();
#endif
    }
}

std::string logging::TradeJournal::DEFAULT_PATH{"trade_journal.jsonl"};

logging::TradeJournal::TradeJournal() : TradeJournal(Config{}) {
}

logging::TradeJournal::TradeJournal(std::filesystem::path path)
    : TradeJournal(Config{
        .path = std::move(path),
        .mode = Mode::OnStop,
        .logger_cpu = std::nullopt
    }) {
}

logging::TradeJournal::TradeJournal(Config config) : config_(std::move(config)) {
    if (config_.mode != Mode::Disabled && config_.path.empty()) {
        throw std::invalid_argument("trade journal path must not be empty");
    }

    stats_path_ = config_.path;
    stats_path_ += ".stats.json";

    if (config_.mode == Mode::RealTime ||
        config_.mode == Mode::RealTimeSeparateThread) {
        real_time_file_.open(config_.path, std::ios::out | std::ios::trunc);
        if (!real_time_file_) {
            throw std::runtime_error("failed to open trade journal: " + config_.path.string());
        }
    }

    if (config_.mode == Mode::RealTimeSeparateThread) {
        logger_failed_.store(false, std::memory_order_release);
        logger_exception_ = nullptr;
        producer_done_.store(false, std::memory_order_release);
        logger_startup_state_.store(
            LoggerStartupState::Starting, std::memory_order_release);
        logger_thread_ = std::jthread(&TradeJournal::log_loop, this);

        auto startup_state = logger_startup_state_.load(std::memory_order_acquire);
        while (startup_state == LoggerStartupState::Starting) {
            logger_startup_state_.wait(
                LoggerStartupState::Starting, std::memory_order_acquire);
            startup_state = logger_startup_state_.load(std::memory_order_acquire);
        }

        if (startup_state == LoggerStartupState::Failed) {
            if (logger_thread_.joinable()) {
                logger_thread_.join();
            }
            std::rethrow_exception(logger_exception_);
        }
    }
}

logging::TradeJournal::~TradeJournal() {
    try {
        stop();
    } catch (...) {
        // Destructors must not throw. Explicit stop() still reports persistence errors.
    }
}

void logging::TradeJournal::add_record(const TradeJournalRecord &record) {
    if (config_.mode == Mode::Disabled) {
        return;
    }

    if (producer_done_.load(std::memory_order_acquire)) {
        throw std::runtime_error("trade journal is already locked for writing");
    }

    if (record.sequence >= next_sequence_) {
        next_sequence_ = record.sequence + 1;
    }
    try_accept(record);
}

void logging::TradeJournal::add_trade(const sim::SimulatedTrade &trade) {
    if (config_.mode == Mode::Disabled) {
        return;
    }

    if (producer_done_.load(std::memory_order_acquire)) {
        throw std::runtime_error("trade journal is already locked for writing");
    }

    const auto new_record = TradeJournalRecord{.sequence = next_sequence_, .trade = trade};
    next_sequence_++;
    try_accept(new_record);
}

bool logging::TradeJournal::try_accept(const TradeJournalRecord &record) {
    switch (config_.mode) {
        case Mode::Disabled:
            return false;
        case Mode::OnStop:
            records_.emplace_back(record);
            ++accepted_records_;
            return true;
        case Mode::RealTime:
            ++accepted_records_;
            append_record(record);
            return true;
        case Mode::RealTimeSeparateThread:
            if (logger_failed_.load(std::memory_order_acquire) ||
                !queue_.try_push(record)) {
                ++dropped_records_;
                return false;
            }
            ++accepted_records_;
            notify_logger();
            return true;
    }
    return false;
}

void logging::TradeJournal::persist_buffered() {
    std::ofstream file(config_.path, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("failed to open trade journal: " + config_.path.string());
    }

    for (const auto &record: records_) {
        file << nlohmann::json(record).dump() << '\n';
    }
    file.close();
    if (!file) {
        throw std::runtime_error("failed to write trade journal: " + config_.path.string());
    }

    written_records_.store(records_.size(), std::memory_order_release);
    write_stats();
}

void logging::TradeJournal::stop() {
    if (lifecycle_state_ == LifecycleState::Failed) {
        std::rethrow_exception(terminal_exception_);
    }
    if (lifecycle_state_ == LifecycleState::Stopped) {
        return;
    }

    producer_done_.store(true, std::memory_order_release);
    notify_logger();

    if (config_.mode == Mode::Disabled) {
        lifecycle_state_ = LifecycleState::Stopped;
        return;
    }

    try {
        switch (config_.mode) {
            case Mode::Disabled:
                break;
            case Mode::OnStop:
                persist_buffered();
                break;
            case Mode::RealTime:
                finish_synchronous_real_time();
                break;
            case Mode::RealTimeSeparateThread:
                finish_real_time();
                break;
        }
        lifecycle_state_ = LifecycleState::Stopped;
    } catch (...) {
        logger_failed_.store(true, std::memory_order_release);
        terminal_exception_ = std::current_exception();
        lifecycle_state_ = LifecycleState::Failed;
        std::rethrow_exception(terminal_exception_);
    }
}

logging::TradeJournal::Stats logging::TradeJournal::stats() const noexcept {
    return Stats{
        .records_attempted = accepted_records_ + dropped_records_,
        .records_accepted = accepted_records_,
        .records_dropped = dropped_records_,
        .records_written = written_records_.load(std::memory_order_acquire),
        .queue_capacity = QUEUE_CAPACITY,
        .logger_failed = logger_failed_.load(std::memory_order_acquire)
    };
}

const std::filesystem::path &logging::TradeJournal::stats_path() const noexcept {
    return stats_path_;
}

void logging::TradeJournal::log_loop() {
    try {
        if (config_.logger_cpu) {
            common::pin_current_thread(*config_.logger_cpu);
        }

        logger_startup_state_.store(
            LoggerStartupState::Ready, std::memory_order_release);
        logger_startup_state_.notify_all();

        while (true) {
            TradeJournalRecord record;
            while (queue_.try_pop(record)) {
                append_record(record);
            }

            if (producer_done_.load(std::memory_order_acquire)) {
                break;
            }

            bool work_available = false;
            for (std::size_t i = 0; i < config_.idle_spin_count; ++i) {
                if (producer_done_.load(std::memory_order_acquire) ||
                    !queue_.empty()) {
                    work_available = true;
                    break;
                }
                cpu_relax();
            }
            if (work_available) {
                continue;
            }

            const auto generation = work_generation_.load(std::memory_order_acquire);
            if (producer_done_.load(std::memory_order_acquire) ||
                !queue_.empty()) {
                continue;
            }
            work_generation_.wait(generation, std::memory_order_acquire);
        }

        TradeJournalRecord record;
        while (queue_.try_pop(record)) {
            append_record(record);
        }

        real_time_file_.close();
        if (!real_time_file_) {
            throw std::runtime_error("failed to write trade journal: " + config_.path.string());
        }
    } catch (...) {
        logger_exception_ = std::current_exception();
        logger_failed_.store(true, std::memory_order_release);
        logger_startup_state_.store(
            LoggerStartupState::Failed, std::memory_order_release);
        logger_startup_state_.notify_all();
    }
}

void logging::TradeJournal::append_record(const TradeJournalRecord &record) {
    real_time_file_ << nlohmann::json(record).dump() << '\n';
    real_time_file_.flush();
    if (!real_time_file_) {
        throw std::runtime_error("failed to write trade journal: " + config_.path.string());
    }
    written_records_.fetch_add(1, std::memory_order_release);
}

void logging::TradeJournal::finish_synchronous_real_time() {
    real_time_file_.close();
    if (!real_time_file_) {
        throw std::runtime_error("failed to write trade journal: " +
                                 config_.path.string());
    }
    write_stats();
}

void logging::TradeJournal::finish_real_time() {
    std::exception_ptr failure;

    try {
        if (logger_thread_.joinable()) {
            logger_thread_.join();
        }
    } catch (...) {
        failure = std::current_exception();
    }

    if (!failure && logger_exception_) {
        failure = logger_exception_;
    }

    // The producer-owned counters are plain integers. Joining establishes
    // that the logger is finished before the final snapshot is persisted.
    try {
        write_stats();
    } catch (...) {
        if (!failure) {
            failure = std::current_exception();
        }
    }

    if (failure) {
        std::rethrow_exception(failure);
    }
}

void logging::TradeJournal::notify_logger() noexcept {
    work_generation_.fetch_add(1, std::memory_order_release);
    work_generation_.notify_one();
}

void logging::TradeJournal::write_stats() const {
    const auto snapshot = stats();
    const nlohmann::json json{
        {"records_attempted", snapshot.records_attempted},
        {"records_accepted", snapshot.records_accepted},
        {"records_dropped", snapshot.records_dropped},
        {"records_written", snapshot.records_written},
        {"queue_capacity", snapshot.queue_capacity},
        {"logger_failed", snapshot.logger_failed}
    };

    std::ofstream file(stats_path_, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("failed to open trade journal stats: " +
                                 stats_path_.string());
    }

    file << json.dump(2) << '\n';
    file.close();
    if (!file) {
        throw std::runtime_error("failed to write trade journal stats: " +
                                 stats_path_.string());
    }
}
