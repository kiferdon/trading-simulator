//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_SIMULATOR_HPP
#define HFT_SIMULATOR_SIMULATOR_HPP

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "input/input_source.hpp"
#include "market_data/itch_parser.hpp"
#include "simulator/market.hpp"
#include "simulator/strategy.hpp"

namespace sim {
    class Simulator {
    public:
        Simulator(std::unique_ptr<IInputSource> input,
                  std::unique_ptr<IStrategy> strategy);

        ~Simulator();

        void run();

        md::ParseOneResult process_one(md::Cursor &cursor);

        void stop();

        void add_tracked_stock(dt::StockLocate locate);

        void set_event_throttle(std::optional<dt::StockLocate> locate,
                                uint32_t every_n_events);

        void set_strategy_simulator();

        void save_trade(const SimulatedTrade &trade);

        // Called by Market via event callback
        void on_market_event(const sim::MarketEvent &event);

    private:
        void process_data(const char *data, size_t size);

        std::unique_ptr<IInputSource> input_;
        std::unique_ptr<IStrategy> strategy_;
        md::ITCHParser parser_;
        std::unique_ptr<Market> market_;
        std::vector<SimulatedTrade> trades_;
        std::ofstream trade_file_;
        std::unordered_set<dt::StockLocate> tracked_stocks_;
        bool running_ = false;
    };
} // namespace sim

#endif // HFT_SIMULATOR_SIMULATOR_HPP
