//
// Created by Claude on 2026-05-29.
//

#ifndef HFT_SIMULATOR_MARKET_EVENT_HPP
#define HFT_SIMULATOR_MARKET_EVENT_HPP

#include <cstdint>

namespace sim {
    using Timestamp = uint64_t;

    struct TopOfBook {
        uint32_t best_bid = 0;
        uint32_t best_bid_qty = 0;
        uint32_t best_ask = 0;
        uint32_t best_ask_qty = 0;
    };

    struct MarketEvent {
        Timestamp timestamp;
        uint16_t stock_locate;
        TopOfBook snapshot;
    };
}

#endif //HFT_SIMULATOR_MARKET_EVENT_HPP