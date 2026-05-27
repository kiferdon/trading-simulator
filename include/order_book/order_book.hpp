#ifndef ORDER_BOOK_ORDER_BOOK_HPP
#define ORDER_BOOK_ORDER_BOOK_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace ob {
    enum class Side { Bid, Ask };

    using OrderId = uint64_t;
    using Price = uint32_t;
    using Volume = int64_t;

    class Order {
    public:
        OrderId order_id;
        Price price;
        Volume volume;

        Order() : order_id(0), price(0), volume(0) {
        }

        Order(const OrderId id, const Price p, const Volume v) : order_id(id), price(p), volume(v) {
        }

        void reset() {
            order_id = 0;
            price = 0;
            volume = 0;
        }
    };

    class OrderBook {
    public:
        // ACCORDING TO PARSER API
        inline void on_add_order(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref, char side, uint32_t shares,
                                 uint64_t stock, uint32_t price) {
        }

        // F: Add Order with MPID Attribution
        inline void on_add_order_with_mpid(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                           uint64_t order_ref, char side, uint32_t shares,
                                           uint64_t stock, uint32_t price, uint32_t mpid) {
        }

        // E: Order Executed
        inline void on_order_executed(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                      uint64_t order_ref, uint32_t executed_shares, uint64_t match_id) {
        }

        // C: Order Executed With Price
        inline void on_order_executed_with_price(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                                 uint64_t order_ref, uint32_t executed_shares,
                                                 uint64_t match_id, char printable, uint32_t price) {
        }

        // X: Order Cancel
        inline void on_order_cancel(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                    uint64_t order_ref, uint32_t canceled_shares) {
        }

        // D: Order Delete
        inline void on_order_delete(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                    uint64_t order_ref) {
        }

        // U: Order Replace
        inline void on_order_replace(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                     uint64_t original_order_ref, uint64_t new_order_ref,
                                     uint32_t shares, uint32_t price) {
        }
    };
}

#endif //ORDER_BOOK_ORDER_BOOK_HPP
