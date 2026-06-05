//
// Created by Claude on 2026-05-29.
//

#ifndef HFT_SIMULATOR_STRATEGY_HPP
#define HFT_SIMULATOR_STRATEGY_HPP

#include <nlohmann/json.hpp>
#include "market_event.hpp"

namespace sim {
    enum class Side { Buy, Sell };

    struct SimulatedTrade {
        Timestamp timestamp;
        Side side;
        uint32_t price;
        uint32_t quantity;
        uint16_t stock_locate;
    };

    class Simulator;  // Forward declaration

    class IStrategy {
    public:
        virtual ~IStrategy() = default;
        virtual void on_event(const MarketEvent& event) = 0;
        virtual void on_trade(const SimulatedTrade& trade) = 0;
        virtual void set_simulator(Simulator* sim) {}
    };
}

namespace nlohmann {
    template <>
    struct adl_serializer<sim::Side> {
        static void to_json(json& j, const sim::Side& side) {
            j = (side == sim::Side::Buy) ? "buy" : "sell";
        }

        static void from_json(const json& j, sim::Side& side) {
            side = (j.get<std::string>() == "buy") ? sim::Side::Buy : sim::Side::Sell;
        }
    };

    template <>
    struct adl_serializer<sim::SimulatedTrade> {
        static void to_json(json& j, const sim::SimulatedTrade& trade) {
            j = json{{"timestamp", trade.timestamp},
                     {"side", trade.side},
                     {"price", trade.price},
                     {"quantity", trade.quantity},
                     {"stock_locate", trade.stock_locate}};
        }

        static void from_json(const json& j, sim::SimulatedTrade& trade) {
            trade.timestamp = j.at("timestamp").get<sim::Timestamp>();
            trade.side = j.at("side").get<sim::Side>();
            trade.price = j.at("price").get<uint32_t>();
            trade.quantity = j.at("quantity").get<uint32_t>();
            trade.stock_locate = j.at("stock_locate").get<uint16_t>();
        }
    };
}

#endif //HFT_SIMULATOR_STRATEGY_HPP