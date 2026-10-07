//
// Created by silay on 5/26/26.
//

#include "simulator/simulator.hpp"

#include <stdexcept>
#include <chrono>
#include <fstream>
#include <utility>

#include "common/thread_affinity.hpp"

namespace sim {
    Simulator::Simulator(std::unique_ptr<IInputSource> input,
                         std::unique_ptr<IStrategy> strategy)
        : Simulator(std::move(input), std::move(strategy), Config{}) {
        legacy_trades_.emplace();
    }

    Simulator::Simulator(std::unique_ptr<IInputSource> input,
                         std::unique_ptr<IStrategy> strategy,
                         Config config)
        : config_(prepare_config(std::move(config))),
          input_(std::move(input)),
          strategy_(std::move(strategy)),
          trade_journal_(config_.trade_journal) {
        market_ = std::make_unique<Market>();
        market_->set_event_handler(this);
        set_strategy_simulator();

        for (const auto locate: config_.tracked_stocks) {
            add_tracked_stock(locate);
        }
        if (config_.event_throttle) {
            set_event_throttle(std::nullopt, *config_.event_throttle);
        }
    }

    Simulator::~Simulator() = default;

    void Simulator::run() {
        run_impl(nullptr);
    }

    void Simulator::run(RunMetrics &metrics) {
        run_impl(&metrics);
    }

    void Simulator::run_impl(RunMetrics *metrics) {
        if (stopped_) {
            throw std::logic_error("a stopped simulator cannot be run again");
        }

        running_ = true;

        using Clock = std::chrono::steady_clock;
        if (metrics) *metrics = {};
        const auto start = metrics ? Clock::now() : Clock::time_point{};

        const char *data = nullptr;
        size_t size = 0;

        while (running_ && input_->read_chunk(data, size)) {
            const auto parsed = parser_.parse(data, size, *market_);
            if (metrics) {
                metrics->parsed += parsed.messages;
                metrics->skipped += parsed.skipped;
            }
        }

        const auto processing_end = metrics ? Clock::now() : Clock::time_point{};
        if (metrics) {
            metrics->input_complete = running_ && parser_.pending_bytes() == 0;
            metrics->journal_at_processing_end = journal_stats();
        }
        const auto finalization_start = metrics ? Clock::now() : Clock::time_point{};
        stop();
        if (metrics) {
            const auto end = Clock::now();
            metrics->processing_seconds = std::chrono::duration<double>(
                processing_end - start).count();
            metrics->finalization_seconds = std::chrono::duration<double>(
                end - finalization_start).count();
            metrics->total_seconds = std::chrono::duration<double>(end - start).count();
        }
    }

    md::ParseOneResult Simulator::process_one(md::Cursor &cursor) {
        return parser_.parse_one(cursor, *market_);
    }

    void Simulator::stop() {
        if (stopped_) {
            return;
        }

        running_ = false;
        if (legacy_trades_ && !legacy_trades_->empty()) {
            std::ofstream file("trades.json", std::ios::out | std::ios::trunc);
            file << nlohmann::json{{"trades", *legacy_trades_}}.dump(2);
            file.close();
            if (!file) {
                throw std::runtime_error("failed to persist legacy trades.json");
            }
        }
        trade_journal_.stop();
        stopped_ = true;
    }

    void Simulator::add_tracked_stock(dt::StockLocate locate) {
        if (tracked_stocks_.contains(locate)) return;
        if (!parser_.add_tracked_stock(locate)) {
            throw std::invalid_argument("tracked stocks exceed parser capacity");
        }
        tracked_stocks_.insert(locate);
        market_->add_tracked_stock(locate);
    }

    void Simulator::set_event_throttle(std::optional<dt::StockLocate> locate,
                                       uint32_t every_n_events) {
        market_->set_event_throttle(locate, every_n_events);
    }

    void Simulator::set_strategy_simulator() {
        if (strategy_) {
            strategy_->set_simulator(this);
        }
    }

    void Simulator::save_trade(const SimulatedTrade &trade) {
        if (stopped_) {
            throw std::logic_error("cannot save a trade after simulator stop");
        }

        if (legacy_trades_) legacy_trades_->push_back(trade);
        if (strategy_) {
            strategy_->on_trade(trade);
        }

        trade_journal_.add_trade(trade);
    }

    logging::TradeJournal::Stats Simulator::journal_stats() const noexcept {
        return trade_journal_.stats();
    }

    const Simulator::Config &Simulator::config() const noexcept {
        return config_;
    }

    Simulator::Config Simulator::prepare_config(Config config) {
        if (config.tracked_stocks.size() > md::ITCHParser::MaxTrackedStocks) {
            throw std::invalid_argument("tracked stocks exceed parser capacity");
        }
        const std::unordered_set<dt::StockLocate> unique(
            config.tracked_stocks.begin(), config.tracked_stocks.end());
        if (unique.size() != config.tracked_stocks.size()) {
            throw std::invalid_argument("tracked stocks contain duplicates");
        }
        if (config.event_throttle && *config.event_throttle == 0) {
            throw std::invalid_argument("event_throttle must be positive");
        }
        if (config.trade_journal.mode ==
                logging::TradeJournal::Mode::RealTimeSeparateThread &&
            config.cpu_affinity && config.trade_journal.logger_cpu &&
            *config.cpu_affinity == *config.trade_journal.logger_cpu) {
            throw std::invalid_argument(
                "cpu_affinity and trade_journal.logger_cpu must differ in "
                "real_time_separate_thread mode");
        }
        if (config.cpu_affinity) {
            common::pin_current_thread(*config.cpu_affinity);
        }
        return config;
    }

    void Simulator::on_market_event(const sim::MarketEvent &event) {
        if (strategy_) {
            strategy_->on_event(event);
        }
    }
} // namespace sim
