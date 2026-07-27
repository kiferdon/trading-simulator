/**
 * @file order_book.hpp
 * @brief Base order book class and common types
 *
 * ## Design
 *
 * The OrderBook class provides a clean separation between:
 * - **Public API**: `add_order()`, `remove_order()`, etc. - the core operations
 * - **ITCH handlers**: `on_add_order()`, `on_order_delete()`, etc. - translate ITCH protocol messages to API calls
 *
 * All ITCH handler methods forward to the public API by default. Subclasses only need to
 * implement the public API; ITCH translation is inherited.
 *
 * ## Complexity
 *
 * | Operation     | Complexity | Notes                          |
 * |---------------|------------|--------------------------------|
 * | add_order     | O(log N)   | tree lookup for price level    |
 * | remove_order  | O(1)       | iterator-based removal         |
 * | modify_order  | O(log N)   | if price changes               |
 * | subtract_order| O(1)       | volume reduction               |
 * | get_snapshot  | O(log N)   | scan to find best bid/ask      |
 *
 * ## Cache Behavior
 *
 * - Uses `std::map` with per-level `std::list` - cache-unfriendly due to scatter-gather
 * - Each order is a heap allocation
 * - Tree traversal involves pointer chasing
 */
#ifndef ORDER_BOOK_ORDER_BOOK_HPP
#define ORDER_BOOK_ORDER_BOOK_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "common/common.hpp"
#include "simulator/market_event.hpp"

namespace ob {
  enum class Side { Bid, Ask };

  inline ob::Side itch_side_to_order_book(char side) {
    return (side == 'B') ? ob::Side::Bid : ob::Side::Ask;
  }

  class Order {
  public:
    dt::OrderId order_id;
    dt::Price price;
    dt::Quantity volume;

    Order() : order_id(0), price(0), volume(0) {
    }

    Order(const dt::OrderId id, const dt::Price p, const dt::Quantity v)
      : order_id(id), price(p), volume(v) {
    }

    void reset() {
      order_id = 0;
      price = 0;
      volume = 0;
    }
  };

  /**
   * @brief Base class for all order book implementations
   *
   * This class defines the public API for order book operations and provides
   * default implementations of ITCH handlers that forward to the public API.
   *
   * Subclasses must implement:
   * - add_order()
   * - remove_order()
   * - modify_order()
   * - subtract_order()
   * - replace_order()
   * - get_snapshot()
   * - print_top_levels()
   *
   * ITCH handlers are inherited and forward to the public API by default.
   */
  class OrderBook {
  public:
    virtual ~OrderBook() = default;

    // ========================================================================
    // PUBLIC API
    // ========================================================================

    /**
     * @brief Add a new order to the book
     * @param order_id Unique order identifier
     * @param price Order price (in cents/ticks)
     * @param volume Number of shares
     * @param side Bid or Ask
     */
    virtual void add_order(dt::OrderId order_id, dt::Price price,
                           dt::Quantity volume, Side side) = 0;

    /**
     * @brief Remove an order completely from the book
     * @param order_id Order to remove
     */
    virtual void remove_order(dt::OrderId order_id) = 0;

    /**
     * @brief Modify an existing order's price and/or volume
     * @param order_id Order to modify
     * @param new_price New price (may be same as current)
     * @param new_volume New volume (may be same as current)
     */
    virtual void modify_order(dt::OrderId order_id, dt::Price new_price,
                              dt::Quantity new_volume) = 0;

    /**
     * @brief Reduce order volume by execution or cancellation
     * @param order_id Order to modify
     * @param volume Amount to reduce (if >= current volume, order is removed)
     */
    virtual void subtract_order(dt::OrderId order_id, dt::Quantity volume) = 0;

    /**
     * @brief Replace one order with another (order ID change)
     * @param original_order_id Order to remove
     * @param new_order_id New order to add
     * @param price Price for new order
     * @param volume Volume for new order
     */
    virtual void replace_order(dt::OrderId original_order_id,
                               dt::OrderId new_order_id, dt::Price price,
                               dt::Quantity volume) = 0;

    /**
     * @brief Get best bid and ask prices with volumes
     */
    virtual sim::TopOfBook get_snapshot() const = 0;

    /**
     * @brief Debug output of top N price levels
     */
    virtual void print_top_levels(std::size_t depth) const = 0;

    // ========================================================================
    // ITCH PROTOCOL HANDLERS
    // ========================================================================
    // These forward to the public API. Override only if needed for special handling.

    virtual void on_add_order(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                              char side, uint32_t shares, uint64_t, uint32_t price) {
      add_order(order_ref, price, shares, itch_side_to_order_book(side));
    }

    virtual void on_add_order_with_mpid(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                                        char side, uint32_t shares, uint64_t,
                                        uint32_t price, uint32_t) {
      add_order(order_ref, price, shares, itch_side_to_order_book(side));
    }

    virtual void on_order_executed(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                                   uint32_t executed_shares, uint64_t) {
      subtract_order(order_ref, executed_shares);
    }

    virtual void on_order_executed_with_price(uint16_t, uint16_t, uint64_t,
                                              uint64_t order_ref,
                                              uint32_t executed_shares, uint64_t, char,
                                              uint32_t) {
      subtract_order(order_ref, executed_shares);
    }

    virtual void on_order_cancel(uint16_t, uint16_t, uint64_t, uint64_t order_ref,
                                 uint32_t canceled_shares) {
      subtract_order(order_ref, canceled_shares);
    }

    virtual void on_order_delete(uint16_t, uint16_t, uint64_t, uint64_t order_ref) {
      remove_order(order_ref);
    }

    virtual void on_order_replace(uint16_t, uint16_t, uint64_t,
                                  uint64_t original_order_ref, uint64_t new_order_ref,
                                  uint32_t shares, uint32_t price) {
      replace_order(original_order_ref, new_order_ref, price, shares);
    }

    // ========================================================================
    // EVENT THROTTLING
    // ========================================================================

    virtual void set_event_throttle(uint32_t interval) {
      event_throttle_interval_ = interval > 0 ? interval : 1;
    }

    virtual uint32_t events_since_last_emit() const { return event_counter_; }

    virtual bool should_emit() const {
      return event_counter_ > 0 && event_counter_ % event_throttle_interval_ == 0;
    }

    virtual void reset_event_counter() { event_counter_ = 0; }

    void increment_event_counter() { ++event_counter_; }

  protected:
    uint32_t event_throttle_interval_ = 1;
    uint32_t event_counter_ = 0;
  };
} // namespace ob

#endif // ORDER_BOOK_ORDER_BOOK_HPP