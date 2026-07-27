/**
 * @file order_book_v2_page_table.hpp
 * @brief OrderBookV2_PageTable - two-level page table for O(1) price access
 *
 * ## Design
 *
 * Uses a two-level page table structure for direct O(1) price level access:
 * - Top-level: vector of page pointers
 * - Page: fixed-size array (65536 entries) of PriceLevel structs
 * - Price to page: `page_index = price >> 16`, `offset = price & 0xFFFF`
 *
 * Storage:
 * - Pages allocated on-demand (only active price ranges consume memory)
 * - `std::deque<IntrusiveOrderNode>` for order node pool (stable addresses)
 * - `std::unordered_map<OrderId, IntrusiveOrderNode*>` for O(1) order lookup
 *
 * ## Complexity
 *
 * | Operation     | Complexity | Notes                                    |
 * |---------------|------------|------------------------------------------|
 * | add_order     | O(1)       | Direct array access, push_front to list  |
 * | remove_order  | O(1)       | Iterator removal, O(1) lookup            |
 * | modify_order  | O(1)       | Usually in-place update                   |
 * | subtract_order| O(1)       | O(1) lookup, volume reduction             |
 * | get_snapshot  | O(1)       | Cached best_bid/best_ask                  |
 *
 * ## Cache Behavior
 *
 * - **Good locality** within page: price levels are contiguous in memory
 * - **Intrusive lists**: order nodes in deque, no per-node allocation
 * - **O(1) access**: no tree traversal, no pointer chasing
 * - **Pre-allocated pages**: memory allocated in chunks
 *
 * ## Memory Usage
 *
 * - Default: pages allocated on-demand
 * - A stock trading around $100 uses only 1-2 pages per side
 * - Each page: 65536 * sizeof(PriceLevel) bytes
 */
#ifndef ORDER_BOOK_ORDER_BOOK_V2_PAGE_TABLE_HPP
#define ORDER_BOOK_ORDER_BOOK_V2_PAGE_TABLE_HPP

#include "order_book.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <limits>
#include <cstdint>
#include <list>

namespace ob {
  // Forward declarations
  struct IntrusiveOrderNode;

  /**
   * @brief Price level with intrusive doubly-linked list
   */
  struct PriceLevel {
    IntrusiveOrderNode* head = nullptr;
    IntrusiveOrderNode* tail = nullptr;
    uint32_t order_count = 0;
    dt::Quantity total_volume = 0;

    void push_front(IntrusiveOrderNode* node);
    void remove(IntrusiveOrderNode* node);
    bool empty() const { return order_count == 0; }
    dt::Quantity volume() const { return total_volume; }
  };

  /**
   * @brief Intrusive order node for use in price level lists
   */
  struct IntrusiveOrderNode {
    dt::OrderId order_id;
    dt::Price price;
    dt::Quantity volume;
    Side side;
    IntrusiveOrderNode* next = nullptr;
    IntrusiveOrderNode* prev = nullptr;

    IntrusiveOrderNode() = default;
    IntrusiveOrderNode(dt::OrderId id, dt::Price p, dt::Quantity v, Side s)
      : order_id(id), price(p), volume(v), side(s) {}
  };

  /**
   * @brief V2 order book using page table for O(1) price access
   *
   * Key features:
   * - O(1) add/remove/modify operations via direct array indexing
   * - Intrusive order nodes in deque (no heap allocation per order)
   * - Cached best bid/ask for O(1) snapshot
   * - Good cache locality within price levels
   */
  class OrderBookV2_PageTable : public OrderBook {
  private:
    // Price mapping
    static constexpr dt::Price PAGE_SIZE = 65536;
    static constexpr uint32_t PAGE_BITS = 16;
    static constexpr dt::Price PRICE_MASK = PAGE_SIZE - 1;

    std::vector<PriceLevel*> bid_pages_;
    std::vector<PriceLevel*> ask_pages_;

    std::deque<IntrusiveOrderNode> order_pool_;
    std::unordered_map<dt::OrderId, IntrusiveOrderNode*> orders_by_id_;

    dt::Price best_bid_ = 0;
    dt::Price best_ask_ = std::numeric_limits<dt::Price>::max();

    PriceLevel& get_level(dt::Price price, Side side);
    [[nodiscard]] const PriceLevel* find_level(dt::Price price, Side side) const;
    [[nodiscard]] dt::Price find_best_bid() const;
    [[nodiscard]] dt::Price find_best_ask() const;
    void refresh_best_after_empty(Side side, dt::Price price);

  public:
    OrderBookV2_PageTable() = default;

    ~OrderBookV2_PageTable() override {
      for (auto* page : bid_pages_) delete[] page;
      for (auto* page : ask_pages_) delete[] page;
    }

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

#endif // ORDER_BOOK_ORDER_BOOK_V2_PAGE_TABLE_HPP
