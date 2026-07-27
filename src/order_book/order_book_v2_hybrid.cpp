//
// OrderBookV2_Hybrid implementation
//
#include "order_book/order_book_v2_hybrid.hpp"
#include <iostream>

namespace ob {
  dt::Price OrderBookV2_Hybrid::find_best_bid() const {
    dt::Price best = 0;
    for (const auto& [price, level] : bids_) {
      if (!level.empty() && price > best) {
        best = price;
      }
    }
    return best;
  }

  dt::Price OrderBookV2_Hybrid::find_best_ask() const {
    dt::Price best = std::numeric_limits<dt::Price>::max();
    for (const auto& [price, level] : asks_) {
      if (!level.empty() && price < best) {
        best = price;
      }
    }
    return best;
  }

  void OrderBookV2_Hybrid::refresh_best_after_empty(Side side, dt::Price price) {
    if (side == Side::Bid && price == best_bid_) {
      best_bid_ = find_best_bid();
    } else if (side == Side::Ask && price == best_ask_) {
      best_ask_ = find_best_ask();
    }
  }

  void OrderBookV2_Hybrid::add_order(
      dt::OrderId order_id,
      dt::Price price,
      dt::Quantity volume,
      Side side) {
    if (volume == 0 || orders_by_id_.contains(order_id)) {
      return;
    }

    HybridOrderNode node(order_id, price, volume, side);

    if (side == Side::Bid) {
      HybridPriceLevel& level = get_bid_level(price);
      level.orders.push_back(node);
      level.total_volume += volume;
      level.order_count = static_cast<uint32_t>(level.orders.size());
      update_best_bid_on_add(price);
      orders_by_id_[order_id] = {side, price, volume, std::prev(level.orders.end())};
    } else {
      HybridPriceLevel& level = get_ask_level(price);
      level.orders.push_back(node);
      level.total_volume += volume;
      level.order_count = static_cast<uint32_t>(level.orders.size());
      update_best_ask_on_add(price);
      orders_by_id_[order_id] = {side, price, volume, std::prev(level.orders.end())};
    }
  }

  void OrderBookV2_Hybrid::remove_order(dt::OrderId order_id) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end()) {
      const HybridOrderLocation location = it->second;
      if (location.side == Side::Bid) {
        auto level_it = bids_.find(location.price);
        if (level_it == bids_.end()) return;
        HybridPriceLevel& level = level_it->second;
        level.orders.erase(location.iterator);
        level.total_volume -= location.volume;
        level.order_count = static_cast<uint32_t>(level.orders.size());
        orders_by_id_.erase(it);
        if (level.empty()) {
          bids_.erase(level_it);
          refresh_best_after_empty(location.side, location.price);
        }
      } else {
        auto level_it = asks_.find(location.price);
        if (level_it == asks_.end()) return;
        HybridPriceLevel& level = level_it->second;
        level.orders.erase(location.iterator);
        level.total_volume -= location.volume;
        level.order_count = static_cast<uint32_t>(level.orders.size());
        orders_by_id_.erase(it);
        if (level.empty()) {
          asks_.erase(level_it);
          refresh_best_after_empty(location.side, location.price);
        }
      }
    }
  }

  void OrderBookV2_Hybrid::modify_order(
      dt::OrderId order_id,
      dt::Price new_price,
      dt::Quantity new_volume) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end()) {
      if (new_volume == 0) {
        remove_order(order_id);
        return;
      }

      HybridOrderLocation& location = it->second;
      if (location.price == new_price) {
        // Only volume changed
        if (location.side == Side::Bid) {
          auto level_it = bids_.find(location.price);
          if (level_it != bids_.end()) {
            level_it->second.total_volume -= location.volume;
            location.iterator->volume = new_volume;
            location.volume = new_volume;
            level_it->second.total_volume += location.volume;
          }
        } else {
          auto level_it = asks_.find(location.price);
          if (level_it != asks_.end()) {
            level_it->second.total_volume -= location.volume;
            location.iterator->volume = new_volume;
            location.volume = new_volume;
            level_it->second.total_volume += location.volume;
          }
        }
      } else {
        // Price changed - remove and re-add
        Side side = location.side;
        remove_order(order_id);
        add_order(order_id, new_price, new_volume, side);
      }
    }
  }

  void OrderBookV2_Hybrid::subtract_order(
      dt::OrderId order_id,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end() && volume > 0) {
      HybridOrderLocation& location = it->second;
      if (volume >= location.volume) {
        remove_order(order_id);
      } else {
        location.volume -= volume;
        location.iterator->volume -= volume;
        if (location.side == Side::Bid) {
          auto level_it = bids_.find(location.price);
          if (level_it != bids_.end()) {
            level_it->second.total_volume -= volume;
          }
        } else {
          auto level_it = asks_.find(location.price);
          if (level_it != asks_.end()) {
            level_it->second.total_volume -= volume;
          }
        }
      }
    }
  }

  void OrderBookV2_Hybrid::replace_order(
      dt::OrderId original_order_id,
      dt::OrderId new_order_id,
      dt::Price price,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(original_order_id);
    if (it != orders_by_id_.end() && !orders_by_id_.contains(new_order_id)) {
      Side side = it->second.side;
      remove_order(original_order_id);
      add_order(new_order_id, price, volume, side);
    }
  }

  void OrderBookV2_Hybrid::print_top_levels(size_t depth) const {
    std::cout << "BIDS (best to worst)\n";
    std::size_t count = 0;
    for (const auto& [price, level] : bids_) {
      if (!level.empty()) {
        std::cout << "Price " << price << ": volume=" << level.volume() << std::endl;
        if (++count >= depth) {
          break;
        }
      }
    }
    std::cout << "ASKS (best to worst)\n";
    count = 0;
    for (const auto& [price, level] : asks_) {
      if (!level.empty()) {
        std::cout << "Price " << price << ": volume=" << level.volume() << std::endl;
        if (++count >= depth) {
          break;
        }
      }
    }
  }

  sim::TopOfBook OrderBookV2_Hybrid::get_snapshot() const {
    sim::TopOfBook tob;
    if (best_bid_ != 0) {
      auto level_it = bids_.find(best_bid_);
      if (level_it != bids_.end()) {
        tob.best_bid = best_bid_;
        tob.best_bid_qty = level_it->second.volume();
      }
    }
    if (best_ask_ != std::numeric_limits<dt::Price>::max()) {
      auto level_it = asks_.find(best_ask_);
      if (level_it != asks_.end()) {
        tob.best_ask = best_ask_;
        tob.best_ask_qty = level_it->second.volume();
      }
    }
    return tob;
  }
} // namespace ob
