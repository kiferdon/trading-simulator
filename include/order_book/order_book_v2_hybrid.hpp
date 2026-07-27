/**
 * @file order_book_v2_hybrid.hpp
 * @brief OrderBookV2_Hybrid - unordered_map for price levels with list storage
 *
 * ## Design
 *
 * Combines multiple data structures for balanced performance:
 * - `std::unordered_map<Price, HybridPriceLevel>` for price level lookup
 * - `std::list<Order>` for FIFO order storage within each level
 * - `std::unordered_map<OrderId, HybridOrderLocation>` for O(1) order lookup and removal
 *
 * This is a hybrid approach: faster than V1's std::map for price lookups,
 * but still uses heap-allocated list nodes for order storage.
 *
 * ## Complexity
 *
 * | Operation     | Complexity | Notes                                    |
 * |---------------|------------|------------------------------------------|
 * | add_order     | O(1) avg   | Hash lookup + list push_back              |
 * | remove_order  | O(1) avg   | Hash lookup + list removal                |
 * | modify_order  | O(1) avg   | Hash lookup                               |
 * | subtract_order| O(1) avg   | Hash lookup                               |
 * | get_snapshot  | O(1)       | Cached best bid/ask                        |
 *
 * ## Cache Behavior
 *
 * - **Medium locality**: hash table buckets may scatter
 * - **Heap allocation**: std::list nodes still allocate individually
 * - **Better than V1**: no tree traversal, just hash lookup
 * - **Worse than V2_PageTable**: per-level list allocations
 *
 * ## When to Use
 *
 * - When price range is very sparse (hash table saves memory)
 * - When you need the simplicity of map-like iteration
 * - When V2_Vector's memory requirements are too high
 */
#ifndef ORDER_BOOK_ORDER_BOOK_V2_HYBRID_HPP
#define ORDER_BOOK_ORDER_BOOK_V2_HYBRID_HPP

#include "order_book.hpp"
#include <vector>
#include <unordered_map>
#include <list>
#include <cstdint>
#include <limits>

namespace ob {
  /**
   * @brief Order node for hybrid implementation
   */
  struct HybridOrderNode {
    dt::OrderId order_id;
    dt::Price price;
    dt::Quantity volume;
    Side side;

    HybridOrderNode() = default;
    HybridOrderNode(dt::OrderId id, dt::Price p, dt::Quantity v, Side s)
      : order_id(id), price(p), volume(v), side(s) {}
  };

  /**
   * @brief Price level with list storage
   */
  struct HybridPriceLevel {
    std::list<HybridOrderNode> orders;
    dt::Quantity total_volume = 0;
    uint32_t order_count = 0;

    bool empty() const { return orders.empty(); }
    size_t size() const { return orders.size(); }
    dt::Quantity volume() const { return total_volume; }
  };

  struct HybridOrderLocation {
    Side side = Side::Bid;
    dt::Price price = 0;
    dt::Quantity volume = 0;
    std::list<HybridOrderNode>::iterator iterator;
  };

  /**
   * @brief V2 order book using hybrid hash map approach
   *
   * Key features:
   * - O(1) average operations via hash map
   * - Good for sparse price distributions
   * - Stable iteration order via std::list
   * - Memory efficient for sparse data
   */
  class OrderBookV2_Hybrid : public OrderBook {
  private:
    std::unordered_map<dt::Price, HybridPriceLevel> bids_;
    std::unordered_map<dt::Price, HybridPriceLevel> asks_;
    std::unordered_map<dt::OrderId, HybridOrderLocation> orders_by_id_;

    dt::Price best_bid_ = 0;
    dt::Price best_ask_ = std::numeric_limits<dt::Price>::max();

    void update_best_bid_on_add(dt::Price price) {
      if (price > best_bid_) best_bid_ = price;
    }

    void update_best_ask_on_add(dt::Price price) {
      if (price < best_ask_) best_ask_ = price;
    }

    HybridPriceLevel& get_bid_level(dt::Price price) {
      return bids_[price];
    }

    HybridPriceLevel& get_ask_level(dt::Price price) {
      return asks_[price];
    }

    [[nodiscard]] dt::Price find_best_bid() const;
    [[nodiscard]] dt::Price find_best_ask() const;
    void refresh_best_after_empty(Side side, dt::Price price);

  public:
    OrderBookV2_Hybrid() = default;
    ~OrderBookV2_Hybrid() override = default;

    void add_order(dt::OrderId order_id,
                   dt::Price price,
                   dt::Quantity volume,
                   Side side) override;

    void remove_order(dt::OrderId order_id) override;

    void modify_order(dt::OrderId order_id,
                     dt::Price new_price,
                     dt::Quantity new_volume) override;

    void subtract_order(dt::OrderId order_id,
                       dt::Quantity volume) override;

    void replace_order(dt::OrderId original_order_id,
                      dt::OrderId new_order_id,
                      dt::Price price,
                      dt::Quantity volume) override;

    void print_top_levels(size_t depth) const override;
    sim::TopOfBook get_snapshot() const override;
  };
} // namespace ob

#endif // ORDER_BOOK_ORDER_BOOK_V2_HYBRID_HPP
