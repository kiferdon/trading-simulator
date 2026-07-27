/**
 * @file order_book_v1.hpp
 * @brief OrderBookV1 - baseline implementation using std::map and std::list
 *
 * ## Design
 *
 * Uses `std::map<Price, std::list<Order>>` for each side of the book:
 * - `std::map` provides O(log N) price level lookup, sorted iteration
 * - `std::list` provides O(1) order insertion/removal within a price level (FIFO)
 * - `std::unordered_map<OrderId, OrderLocation>` for O(1) order lookup by ID
 *
 * ## Complexity
 *
 * | Operation     | Complexity | Notes                                    |
 * |---------------|------------|------------------------------------------|
 * | add_order     | O(log N)   | Tree lookup for price level, push_back   |
 * | remove_order  | O(1)       | Iterator erasure, O(1) lookup by ID      |
 * | modify_order  | O(log N)   | If price changes, erase + insert         |
 * | subtract_order| O(1)       | Order lookup + volume reduction          |
 * | get_snapshot  | O(log N)   | Scan from begin/rbegin for best bid/ask |
 *
 * ## Cache Behavior
 *
 * - **Poor cache locality**: Tree nodes are scattered in memory (pointer chasing)
 * - **Heap allocation per order**: Each `std::list` node is separately allocated
 * - **Tree overhead**: Red-black tree nodes contain 3 pointers + color
 * - **Recommendation**: For better cache performance, use OrderBookV2_* variants
 *
 * ## Known Issues
 *
 * - No explicit duplicate order ID handling; later adds overwrite the lookup entry
 */
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

  /**
   * @brief V1 order book - baseline implementation
   *
   * Uses std::map and std::list for price levels and orders.
   * Simple, correct, but not the most cache-friendly.
   */
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
      auto it = orders_by_id.find(order_id);
      if (it == orders_by_id.end()) return;  // Order not found, ignore

      auto &location = it->second;
      std::list<Order> &level = (location.side == Side::Bid)
                                  ? bids[location.price]
                                  : asks[location.price];

      level.erase(location.iterator);
      orders_by_id.erase(it);

      cleanup_empty_level(location.side, location.price, level);
    }

    void modify_order(dt::OrderId order_id, dt::Price new_price,
                      dt::Quantity new_volume) override {
      auto it = orders_by_id.find(order_id);
      if (it == orders_by_id.end()) return;  // Order not found, ignore

      auto &location = it->second;
      std::list<Order> &level = (location.side == Side::Bid)
                                  ? bids[location.price]
                                  : asks[location.price];

      if (location.price != new_price) {
        const dt::Price old_price = location.price;
        level.erase(location.iterator);
        cleanup_empty_level(location.side, old_price, level);

        std::list<Order> &new_level =
            (location.side == Side::Bid) ? bids[new_price] : asks[new_price];
        new_level.push_back(Order{order_id, new_price, new_volume});
        location.iterator = std::prev(new_level.end());
      } else {
        location.iterator->volume = new_volume;
      }

      location.price = new_price;
    }

    void subtract_order(dt::OrderId order_id, dt::Quantity volume) override {
      auto it = orders_by_id.find(order_id);
      if (it == orders_by_id.end()) return;  // Order not found, ignore

      auto &location = it->second;
      Order &order = *location.iterator;
      bool is_full = volume >= order.volume;
      order.volume -= volume;
      if (is_full) {
        std::list<Order> &level = (location.side == Side::Bid)
                                    ? bids[location.price]
                                    : asks[location.price];
        level.erase(location.iterator);
        orders_by_id.erase(it);

        cleanup_empty_level(location.side, location.price, level);
      }
    }

    void replace_order(dt::OrderId original_order_id, dt::OrderId new_order_id,
                       dt::Price price, dt::Quantity volume) override {
      auto it = orders_by_id.find(original_order_id);
      if (it == orders_by_id.end()) return;  // Order not found, ignore

      Side side = it->second.side;
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

    void print_top_levels(std::size_t depth) const override {
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

  private:
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
  };
} // namespace ob

#endif // ORDER_BOOK_ORDER_BOOK_V1_HPP
