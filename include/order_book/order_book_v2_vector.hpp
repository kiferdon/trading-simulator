/**
 * @file order_book_v2_vector.hpp
 * @brief OrderBookV2_Vector - dense vector storage for price levels
 *
 * ## Design
 *
 * Uses dense `std::vector` indexed by normalized price for storage:
 * - `std::vector<VectorPriceLevel>` for bids and asks
 * - Direct normalized indexing: `levels[(price - min_price) / tick_size]`
 * - Orders stored as `std::vector<OrderId>` within each level
 * - Order lookup stores side, level index, and order index for O(1) removal
 *
 * ## Complexity
 *
 * | Operation     | Complexity | Notes                                    |
 * |---------------|------------|------------------------------------------|
 * | add_order     | O(1)       | Direct access + vector push              |
 * | remove_order  | O(1)       | Hash lookup + swap-pop from level        |
 * | modify_order  | O(1)       | Same-price update or remove/add          |
 * | subtract_order| O(1)       | O(1) volume update or remove             |
 * | get_snapshot  | O(1)       | Cached best bid/ask and level volume     |
 *
 * ## Cache Behavior
 *
 * - **Excellent locality**: contiguous vector storage for price levels
 * - **Cache-friendly iteration**: linear memory access pattern
 * - **No pointer chasing**: unlike map-based implementations
 * - **Trade-off**: fixed bounded range wastes memory for sparse prices
 *
 * ## Memory Usage
 *
 * - Pre-allocated vector for the configured price range
 * - No hot-path resize by raw price
 * - Minimal overhead per price level
 *
 * ## Caveats
 *
 * - Best for price ranges that fit in memory (not unbounded)
 * - Uses swap-pop removal, so FIFO order within a price level is not preserved
 */
#ifndef ORDER_BOOK_ORDER_BOOK_V2_VECTOR_HPP
#define ORDER_BOOK_ORDER_BOOK_V2_VECTOR_HPP

#include "order_book.hpp"
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace ob {
  struct VectorPriceRange {
    dt::Price min_price = 0;
    dt::Price max_price = 1'000'000;
    dt::Price tick_size = 1;
  };

  struct VectorOrderLocation {
    dt::Price price = 0;
    dt::Quantity volume = 0;
    Side side = Side::Bid;
    uint32_t level_index = 0;
    uint32_t order_index = 0;
  };

  /**
   * @brief Price level with vector storage for order IDs
   */
  struct VectorPriceLevel {
    std::vector<dt::OrderId> orders;
    dt::Quantity total_volume = 0;
    uint32_t order_count = 0;

    bool empty() const { return orders.empty(); }
    size_t size() const { return orders.size(); }
    dt::Quantity volume() const { return total_volume; }
  };

  /**
   * @brief V2 order book using dense vector storage
   *
   * Key features:
   * - O(1) access by price via direct array indexing
   * - Contiguous memory layout for cache efficiency
   * - Simple implementation with minimal indirection
   * - Good for price ranges that fit in available memory
   */
  class OrderBookV2_Vector : public OrderBook {
  private:
    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    VectorPriceRange range_;
    std::vector<VectorPriceLevel> bid_levels_;
    std::vector<VectorPriceLevel> ask_levels_;
    std::vector<uint64_t> active_bid_words_;
    std::vector<uint64_t> active_ask_words_;
    std::unordered_map<dt::OrderId, VectorOrderLocation> orders_by_id_;

    uint32_t best_bid_index_ = INVALID_INDEX;
    uint32_t best_ask_index_ = INVALID_INDEX;

    [[nodiscard]] bool price_to_index(dt::Price price, uint32_t& index) const;
    [[nodiscard]] dt::Price index_to_price(uint32_t index) const;
    [[nodiscard]] VectorPriceLevel& level_for(Side side, uint32_t index);
    [[nodiscard]] const VectorPriceLevel& level_for(Side side, uint32_t index) const;

    void set_active(Side side, uint32_t index);
    void clear_active(Side side, uint32_t index);
    [[nodiscard]] bool is_active(Side side, uint32_t index) const;
    [[nodiscard]] uint32_t find_best_bid_index() const;
    [[nodiscard]] uint32_t find_best_ask_index() const;
    void remove_order_at(dt::OrderId order_id, const VectorOrderLocation& location);

  public:
    explicit OrderBookV2_Vector(VectorPriceRange range = VectorPriceRange{});
    ~OrderBookV2_Vector() override = default;

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

#endif // ORDER_BOOK_ORDER_BOOK_V2_VECTOR_HPP
