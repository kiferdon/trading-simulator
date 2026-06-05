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
    dt::Price price;
    std::list<Order>::iterator iterator;
  };

  class OrderBookV1 : public OrderBook {
  private:
    std::map<dt::Price, std::list<Order> > bids;
    std::map<dt::Price, std::list<Order> > asks;

    std::unordered_map<dt::OrderId, OrderLocation> orders_by_id;

  public:
    void add_order(dt::OrderId order_id, dt::Price price, dt::Quantity volume,
                   Side side) override {
      Order order{order_id, price, volume};
      std::list<Order> &level = (side == Side::Bid) ? bids[price] : asks[price];

      level.push_back(order);

      auto iterator = std::prev(level.end());

      orders_by_id[order_id] = {side, price, iterator};
    }

    void remove_order(dt::OrderId order_id) override {
      auto &location = orders_by_id[order_id];
      std::list<Order> &level = (location.side == Side::Bid)
                                  ? bids[location.price]
                                  : asks[location.price];

      level.erase(location.iterator);
      orders_by_id.erase(order_id);

      cleanup_empty_level(location.side, location.price, level);
    }

    void modify_order(dt::OrderId order_id, dt::Price new_price,
                      dt::Quantity new_volume) override {
      auto &location = orders_by_id[order_id];
      std::list<Order> &level = (location.side == Side::Bid)
                                  ? bids[location.price]
                                  : asks[location.price];

      if (location.price != new_price) {
        level.erase(location.iterator);

        std::list<Order> &new_level =
            (location.side == Side::Bid) ? bids[new_price] : asks[new_price];
        new_level.push_back(Order{order_id, new_price, new_volume});
        location.iterator = std::prev(new_level.end());
      }

      location.price = new_price;
    }

    void subtract_order(dt::OrderId order_id, dt::Quantity volume) override {
      auto &location = orders_by_id[order_id];
      Order &order = *location.iterator;
      bool is_full = volume >= order.volume;
      order.volume -= volume;
      if (is_full) {
        std::list<Order> &level = (location.side == Side::Bid)
                                    ? bids[location.price]
                                    : asks[location.price];
        level.erase(location.iterator);
        orders_by_id.erase(order_id);

        cleanup_empty_level(location.side, location.price, level);
      }
    }

    void replace_order(dt::OrderId original_order_id, dt::OrderId new_order_id,
                       dt::Price price, dt::Quantity volume) override {
      auto &location = orders_by_id[original_order_id];
      Side side = location.side;
      remove_order(original_order_id);
      add_order(new_order_id, price, volume, side);
    }

    dt::Quantity aggregate_volume(const std::list<Order> &orders) const {
      dt::Quantity total_volume = 0;
      for (const auto &order: orders) {
        total_volume += order.volume;
      }
      return total_volume;
    }

    void print_top_levels(std::size_t depth) const {
      std::cout << ("ASKS") << std::endl;

      std::size_t count = 0;

      for (const auto &[price, orders]: asks) {
        dt::Quantity total_volume = aggregate_volume(orders);
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
        dt::Quantity total_volume = aggregate_volume(orders);
        if (total_volume == 0) {
          continue;
        }

        std::cout << price << " | " << total_volume << std::endl;

        if (++count >= depth)
          break;
      }

      std::cout << ("BIDS") << std::endl;
    }

    // ITCH handler interface — forwards to OrderBook API
    void on_add_order(uint16_t, uint16_t, uint64_t, uint64_t order_ref, char side,
                      uint32_t shares, uint64_t, uint32_t price) {
      add_order(order_ref, price, shares, itch_side_to_order_book(side));
    }

    void on_add_order_with_mpid(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                                char side, uint32_t shares, uint64_t,
                                uint32_t price, uint32_t) {
      add_order(order_ref, price, shares, itch_side_to_order_book(side));
    }

    void on_order_executed(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                           uint32_t executed_shares, uint64_t) {
      subtract_order(order_ref, executed_shares);
    }

    void on_order_executed_with_price(uint16_t, uint16_t, uint64_t,
                                      uint64_t order_ref,
                                      uint32_t executed_shares, uint64_t, char,
                                      uint32_t) {
      subtract_order(order_ref, executed_shares);
    }

    void on_order_cancel(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                         uint32_t canceled_shares) {
      subtract_order(order_ref, canceled_shares);
    }

    void on_order_delete(uint16_t, uint16_t, uint64_t, uint64_t order_ref) {
      remove_order(order_ref);
    }

    void on_order_replace(uint16_t, uint16_t, uint64_t,
                          uint64_t original_order_ref, uint64_t new_order_ref,
                          uint32_t shares, uint32_t price) {
      replace_order(original_order_ref, new_order_ref, price, shares);
    }

  private:
    // Helper to clean up empty price levels after order removal
    void cleanup_empty_level(Side side, dt::Price price,
                             const std::list<Order> &level) {
      if (level.empty()) {
        if (side == Side::Bid) {
          bids.erase(price);
        } else {
          asks.erase(price);
        }
      }
    }

  public:
    sim::TopOfBook get_snapshot() const override {
      sim::TopOfBook tob;
      // Find highest bid with non-empty orders (iterate in reverse)
      for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
        if (!it->second.empty()) {
          tob.best_bid = it->first;
          tob.best_bid_qty = aggregate_volume(it->second);
          break;
        }
      }
      // Find lowest ask with non-empty orders
      for (const auto &[price, orders]: asks) {
        if (!orders.empty()) {
          tob.best_ask = price;
          tob.best_ask_qty = aggregate_volume(orders);
          break;
        }
      }
      return tob;
    }
  };
} // namespace ob

#endif // ORDER_BOOK_ORDER_BOOK_V1_HPP
