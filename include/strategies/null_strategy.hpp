//
// Created by silay on 8/6/26.
//

#ifndef HFT_SIMULATOR_NULL_STRATEGY_HPP
#define HFT_SIMULATOR_NULL_STRATEGY_HPP
#include "simulator/market_event.hpp"
#include "simulator/strategy.hpp"

namespace sim {
    struct NullStrategy : public sim::IStrategy {
        void on_event(const sim::MarketEvent &) override {
        }

        void on_trade(const sim::SimulatedTrade &) override {
        }
    };
}


#endif //HFT_SIMULATOR_NULL_STRATEGY_HPP
