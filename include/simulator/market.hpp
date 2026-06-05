//
// Created by silay on 6/4/26.
//

#ifndef HFT_SIMULATOR_MARKET_HPP
#define HFT_SIMULATOR_MARKET_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "common/common.hpp"
#include "order_book/order_book.hpp"

namespace sim {
  class Simulator;
}

class Market {
public:
  // ITCH callbacks — called by the parser
  void on_add_order(uint16_t locate, uint16_t, uint64_t, uint64_t order_ref,
                    char side, uint32_t shares, uint64_t, uint32_t price);

  void on_add_order_with_mpid(uint16_t locate, uint16_t, uint64_t,
                              uint64_t order_ref, char side, uint32_t shares,
                              uint64_t, uint32_t price, uint32_t);

  void on_order_executed(uint16_t locate, uint16_t, uint64_t,
                         uint64_t order_ref, uint32_t executed_shares,
                         uint64_t);

  void on_order_executed_with_price(uint16_t locate, uint16_t, uint64_t,
                                    uint64_t order_ref,
                                    uint32_t executed_shares, uint64_t, char,
                                    uint32_t);

  void on_order_cancel(uint16_t locate, uint16_t, uint64_t, uint64_t order_ref,
                       uint32_t canceled_shares);

  void on_order_delete(uint16_t locate, uint16_t, uint64_t, uint64_t order_ref);

  void on_order_replace(uint16_t locate, uint16_t, uint64_t,
                        uint64_t original_order_ref, uint64_t new_order_ref,
                        uint32_t shares, uint32_t price);

  // Called internally after every ITCH event
  void on_market_event(uint64_t timestamp, uint16_t locate);

  // Simulator calls this at construction to receive market events
  void set_event_handler(sim::Simulator *handler);

  // API
  void add_tracked_stock(dt::StockLocate locate);

  void set_event_throttle(std::optional<dt::StockLocate> locate,
                          uint32_t every_n_events);

  const std::unordered_map<dt::StockLocate, std::unique_ptr<ob::OrderBook> > &
  order_books() const {
    return order_books_;
  }

private:
  ob::OrderBook *get_order_book(dt::StockLocate locate);

  std::unordered_map<dt::StockLocate, std::unique_ptr<ob::OrderBook> >
  order_books_;
  std::unordered_set<dt::StockLocate> tracked_stocks_;
  sim::Simulator *event_handler_ = nullptr;
};

#endif // HFT_SIMULATOR_MARKET_HPP
