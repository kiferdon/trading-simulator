//
// Created by silay on 5/19/26.
//

#ifndef ORDER_BOOK_ORDER_BOOK_V1_HPP
#define ORDER_BOOK_ORDER_BOOK_V1_HPP
#include <iostream>
#include <list>
#include <map>
#include <ranges>

#include "order_book.hpp"

namespace ob {
    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator iterator;
    };

    class OrderBookV1 : public OrderBook {
    private:
        std::map<Price, std::list<Order> > bids;
        std::map<Price, std::list<Order> > asks;

        std::unordered_map<OrderId, OrderLocation> orders_by_id;

    public:
        inline void on_add_order(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref, char side, uint32_t shares,
                                 uint64_t stock, uint32_t price) {
            //checksum += timestamp;
            //AddOrder(side == 'B' ? Side::Bid : Side::Ask, price, shares);
            //add_order({order_ref, price, shares}, side == 'B');
            add_order(order_ref, price, shares, side == 'B' ? Side::Bid : Side::Ask);
        }

        // F: Add Order with MPID Attribution
        inline void on_add_order_with_mpid(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                           uint64_t order_ref, char side, uint32_t shares,
                                           uint64_t stock, uint32_t price, uint32_t mpid) {
            // Note: MPID is usually 4 alpha chars, often handled as uint32_t
            //checksum += timestamp;
            add_order(order_ref, price, shares, side == 'B' ? Side::Bid : Side::Ask);
        }

        // E: Order Executed
        inline void on_order_executed(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                      uint64_t order_ref, uint32_t executed_shares, uint64_t match_id) {
            //checksum += timestamp;
            //update_order({order_ref, price})
            subtract_order(order_ref, executed_shares);
        }

        // C: Order Executed With Price
        inline void on_order_executed_with_price(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                                 uint64_t order_ref, uint32_t executed_shares,
                                                 uint64_t match_id, char printable, uint32_t price) {
            //checksum += timestamp;
            subtract_order(order_ref, executed_shares);
        }

        // X: Order Cancel
        inline void on_order_cancel(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                    uint64_t order_ref, uint32_t canceled_shares) {
            //checksum += timestamp;
            subtract_order(order_ref, canceled_shares);
        }

        // D: Order Delete
        inline void on_order_delete(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                    uint64_t order_ref) {
            //checksum += timestamp;
            remove_order(order_ref);
        }

        // U: Order Replace
        inline void on_order_replace(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                     uint64_t original_order_ref, uint64_t new_order_ref,
                                     uint32_t shares, uint32_t price) {
            //checksum += timestamp;
            auto &location = orders_by_id[original_order_ref];
            Side side = location.side;
            remove_order(original_order_ref);
            add_order(new_order_ref, price, shares, side);
        }

        void add_order(OrderId order_id, Price price, Volume volume, Side side) {
            Order order{order_id, price, volume};
            std::list<Order> &level = (side == Side::Bid) ? bids[price] : asks[price];

            level.push_back(order);

            auto iterator = std::prev(level.end());

            orders_by_id[order_id] = {
                side,
                price,
                iterator
            };
        }

        void remove_order(OrderId order_id) {
            auto &location = orders_by_id[order_id];
            std::list<Order> &level = (location.side == Side::Bid) ? bids[location.price] : asks[location.price];

            level.erase(location.iterator);
            orders_by_id.erase(order_id);
        }

        void modify_order(OrderId order_id, Price new_price, Volume new_volume) {
            auto &location = orders_by_id[order_id];
            std::list<Order> &level = (location.side == Side::Bid) ? bids[location.price] : asks[location.price];

            if (location.price != new_price) {
                level.erase(location.iterator);

                std::list<Order> &new_level = (location.side == Side::Bid) ? bids[new_price] : asks[new_price];
                new_level.push_back(Order{order_id, new_price, new_volume});
                location.iterator = std::prev(new_level.end());
            }

            location.price = new_price;
        }

        void subtract_order(OrderId order_id, Volume volume) {
            auto &location = orders_by_id[order_id];
            Order &order = *location.iterator;
            bool is_full = volume >= order.volume;
            order.volume -= volume;
            if (is_full) {
                std::list<Order> &level = (location.side == Side::Bid) ? bids[location.price] : asks[location.price];
                level.erase(location.iterator);
                orders_by_id.erase(order_id);
            }
        }

        Volume aggregate_volume(const std::list<Order> &orders) const {
            Volume total_volume = 0;
            for (const auto &order: orders) {
                total_volume += order.volume;
            }
            return total_volume;
        }

        void print_top_levels(std::size_t depth) const {
            std::cout << ("ASKS") << std::endl;

            std::size_t count = 0;

            for (const auto &[price, orders]: asks) {
                Volume total_volume = aggregate_volume(orders);
                if (total_volume == 0) {
                    continue;
                }

                std::cout << price << " | " << total_volume << std::endl;
                if (++count >= depth)
                    break;
            }

            std::cout << ("----------------") << std::endl;

            count = 0;

            for (const auto &[price, orders]: std::views::reverse(bids)) {
                Volume total_volume = aggregate_volume(orders);
                if (total_volume == 0) {
                    continue;
                }

                std::cout << price << " | " << total_volume << std::endl;

                if (++count >= depth)
                    break;
            }

            std::cout << ("BIDS") << std::endl;
        }
    };
}

#endif //ORDER_BOOK_ORDER_BOOK_V1_HPP
