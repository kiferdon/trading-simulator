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

  class OrderBook {
  public:
    virtual ~OrderBook() = default;

    // PUBLIC API
    virtual void add_order(dt::OrderId order_id, dt::Price price,
                           dt::Quantity volume, Side side) {
    }

    virtual void remove_order(dt::OrderId order_id) {
    }

    virtual void modify_order(dt::OrderId order_id, dt::Price new_price,
                              dt::Quantity new_volume) {
    }

    virtual void subtract_order(dt::OrderId order_id, dt::Quantity volume) {
    }

    virtual void replace_order(dt::OrderId original_order_id,
                               dt::OrderId new_order_id, dt::Price price,
                               dt::Quantity volume) {
    }

    // Snapshot and event throttling
    virtual sim::TopOfBook get_snapshot() const { return sim::TopOfBook{}; }

    virtual sim::MarketEvent get_market_event(uint64_t timestamp,
                                              uint16_t stock_locate) {
      sim::MarketEvent event;
      event.timestamp = timestamp;
      event.stock_locate = stock_locate;
      event.snapshot = get_snapshot();
      return event;
    }

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
