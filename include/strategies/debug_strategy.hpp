//
// Created by silay on 8/6/26.
//

#ifndef HFT_SIMULATOR_DEBUG_STRATEGY_HPP
#define HFT_SIMULATOR_DEBUG_STRATEGY_HPP
#include <fstream>

#include "simulator/simulator.hpp"
#include "simulator/strategy.hpp"

namespace sim {
    class DebugStrategy : public sim::IStrategy {
    public:
        enum class FlushMode { RealTime, OnFinish };

        struct Config {
            bool log_events = false;
            bool log_trades = false;
            FlushMode flush_mode = FlushMode::RealTime;
            std::string log_file_path;
        };

        explicit DebugStrategy(Config config) : config_(std::move(config)) {
            if (config_.log_events || config_.log_trades) {
                log_file_.open(config_.log_file_path, std::ios::out | std::ios::trunc);
            }
        }

        ~DebugStrategy() override {
            if (config_.flush_mode == FlushMode::OnFinish && log_file_.is_open()) {
                for (const auto &line: log_buffer_) {
                    log_file_ << line;
                }
                log_buffer_.clear();
            }
            log_file_.close();
        }

        void on_event(const sim::MarketEvent &event) override {
            constexpr uint32_t SPREAD_THRESHOLD_PCT = 20; // 20 = 2.0%

            checksum += event.timestamp;

            if (config_.log_events) {
                std::string log = "[EVENT] time=" + std::to_string(event.timestamp)
                                  + " stock=" + std::to_string(event.stock_locate)
                                  + " bid=" + std::to_string(event.snapshot.best_bid)
                                  + " ask=" + std::to_string(event.snapshot.best_ask) + "\n";

                if (config_.flush_mode == FlushMode::RealTime) {
                    log_file_ << log;
                } else {
                    log_buffer_.push_back(log);
                }
            }

            if (event.snapshot.best_bid > 0 &&
                event.snapshot.best_ask > event.snapshot.best_bid) {
                uint32_t spread = event.snapshot.best_ask - event.snapshot.best_bid;
                uint64_t mid = (static_cast<uint64_t>(event.snapshot.best_bid) +
                                event.snapshot.best_ask) /
                               2;

                // Avoid overflow: compare as scaled integers (spread/ask and spread/bid)
                if (spread * 100 < static_cast<uint32_t>(mid) * SPREAD_THRESHOLD_PCT) {
                    decide_to_trade(event, spread);
                }
            }
        }

        void on_trade(const sim::SimulatedTrade &trade) override {
            ++trades;
            checksum += trade.timestamp;
            checksum += trade.price;
            checksum += trade.quantity;

            if (!config_.log_trades) {
                return;
            }

            std::string log = ("[TRADE] time=" + std::to_string(trade.timestamp) +
                               " side=" + (trade.side == sim::Side::Buy ? "BUY" : "SELL")
                               + " price=" + std::to_string(trade.price) + " qty=" + std::to_string(trade.quantity) +
                               "\n");

            if (config_.flush_mode == FlushMode::RealTime) {
                log_file_ << log;
            } else {
                log_buffer_.push_back(log);
            }
        }

        void set_simulator(sim::Simulator *sim) override { simulator_ = sim; }

    private:
        void decide_to_trade(const sim::MarketEvent &event, uint32_t spread) {
            if (!simulator_)
                return;

            sim::SimulatedTrade trade{};
            trade.timestamp = event.timestamp;
            trade.quantity = 100;
            trade.stock_locate = event.stock_locate;
            trade.side = buy_next_ ? sim::Side::Buy : sim::Side::Sell;
            buy_next_ = !buy_next_;
            trade.price = (trade.side == sim::Side::Buy)
                              ? event.snapshot.best_ask
                              : event.snapshot.best_bid;

            simulator_->save_trade(trade);
        }

    public:
        uint64_t checksum = 0;
        size_t trades = 0;

        Config config_;
        std::ofstream log_file_;
        sim::Simulator *simulator_ = nullptr;
        std::vector<std::string> log_buffer_;
        bool buy_next_ = true;
    };
}

#endif //HFT_SIMULATOR_DEBUG_STRATEGY_HPP
