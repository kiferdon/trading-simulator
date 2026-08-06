//
// Created by silay on 8/6/26.
//

#ifndef HFT_SIMULATOR_TRADING_STRATEGY_HPP
#define HFT_SIMULATOR_TRADING_STRATEGY_HPP
#include "simulator/simulator.hpp"
#include "simulator/strategy.hpp"

class TradingStrategy : public sim::IStrategy {
public:
    void on_event(const sim::MarketEvent &event) override {
        checksum += event.timestamp;

        if (event.snapshot.best_bid == 0 ||
            event.snapshot.best_ask <= event.snapshot.best_bid) {
            return;
        }

        sim::SimulatedTrade trade{};
        trade.timestamp = event.timestamp;
        trade.quantity = 100;
        trade.stock_locate = event.stock_locate;
        trade.side = buy_next_ ? sim::Side::Buy : sim::Side::Sell;
        buy_next_ = !buy_next_;
        trade.price = trade.side == sim::Side::Buy
                          ? event.snapshot.best_ask
                          : event.snapshot.best_bid;

        if (simulator_ != nullptr) {
            simulator_->save_trade(trade);
        }
    }

    void on_trade(const sim::SimulatedTrade &trade) override {
        ++trades;
        checksum += trade.timestamp;
        checksum += trade.price;
        checksum += trade.quantity;
    }

    void set_simulator(sim::Simulator *simulator) override {
        simulator_ = simulator;
    }

    size_t trades = 0;
    uint64_t checksum = 0;

private:
    sim::Simulator *simulator_ = nullptr;
    bool buy_next_ = true;
};

#endif //HFT_SIMULATOR_TRADING_STRATEGY_HPP
