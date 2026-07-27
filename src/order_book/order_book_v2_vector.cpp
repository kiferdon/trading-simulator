//
// OrderBookV2_Vector implementation
//
#include "order_book/order_book_v2_vector.hpp"

#include <cassert>
#include <bit>
#include <iostream>

namespace ob {
  namespace {
    constexpr uint32_t BITS_PER_WORD = 64;

    uint32_t level_count(const VectorPriceRange& range) {
      assert(range.tick_size > 0);
      assert(range.max_price >= range.min_price);
      return ((range.max_price - range.min_price) / range.tick_size) + 1;
    }
  }

  OrderBookV2_Vector::OrderBookV2_Vector(VectorPriceRange range)
      : range_(range),
        bid_levels_(level_count(range)),
        ask_levels_(level_count(range)),
        active_bid_words_((bid_levels_.size() + BITS_PER_WORD - 1) / BITS_PER_WORD, 0),
        active_ask_words_((ask_levels_.size() + BITS_PER_WORD - 1) / BITS_PER_WORD, 0) {
  }

  bool OrderBookV2_Vector::price_to_index(dt::Price price, uint32_t& index) const {
    const bool in_range = price >= range_.min_price && price <= range_.max_price;
    const bool aligned = in_range && ((price - range_.min_price) % range_.tick_size == 0);
    assert(in_range && aligned);
    if (!in_range || !aligned) {
      return false;
    }

    index = (price - range_.min_price) / range_.tick_size;
    return true;
  }

  dt::Price OrderBookV2_Vector::index_to_price(uint32_t index) const {
    return range_.min_price + index * range_.tick_size;
  }

  VectorPriceLevel& OrderBookV2_Vector::level_for(Side side, uint32_t index) {
    return side == Side::Bid ? bid_levels_[index] : ask_levels_[index];
  }

  const VectorPriceLevel& OrderBookV2_Vector::level_for(Side side, uint32_t index) const {
    return side == Side::Bid ? bid_levels_[index] : ask_levels_[index];
  }

  void OrderBookV2_Vector::set_active(Side side, uint32_t index) {
    auto& words = side == Side::Bid ? active_bid_words_ : active_ask_words_;
    words[index / BITS_PER_WORD] |= uint64_t{1} << (index % BITS_PER_WORD);
  }

  void OrderBookV2_Vector::clear_active(Side side, uint32_t index) {
    auto& words = side == Side::Bid ? active_bid_words_ : active_ask_words_;
    words[index / BITS_PER_WORD] &= ~(uint64_t{1} << (index % BITS_PER_WORD));
  }

  bool OrderBookV2_Vector::is_active(Side side, uint32_t index) const {
    const auto& words = side == Side::Bid ? active_bid_words_ : active_ask_words_;
    return (words[index / BITS_PER_WORD] & (uint64_t{1} << (index % BITS_PER_WORD))) != 0;
  }

  uint32_t OrderBookV2_Vector::find_best_bid_index() const {
    for (uint32_t word_index = static_cast<uint32_t>(active_bid_words_.size()); word_index > 0;) {
      --word_index;
      const uint64_t word = active_bid_words_[word_index];
      if (word == 0) {
        continue;
      }

      const uint32_t bit = 63U - static_cast<uint32_t>(std::countl_zero(word));
      return word_index * BITS_PER_WORD + bit;
    }

    return INVALID_INDEX;
  }

  uint32_t OrderBookV2_Vector::find_best_ask_index() const {
    for (uint32_t word_index = 0; word_index < active_ask_words_.size(); ++word_index) {
      const uint64_t word = active_ask_words_[word_index];
      if (word == 0) {
        continue;
      }

      const uint32_t bit = static_cast<uint32_t>(std::countr_zero(word));
      return word_index * BITS_PER_WORD + bit;
    }

    return INVALID_INDEX;
  }

  void OrderBookV2_Vector::remove_order_at(dt::OrderId order_id,
                                           const VectorOrderLocation& location) {
    VectorPriceLevel& level = level_for(location.side, location.level_index);
    const uint32_t last_index = static_cast<uint32_t>(level.orders.size() - 1);

    if (location.order_index != last_index) {
      const dt::OrderId moved_order_id = level.orders[last_index];
      level.orders[location.order_index] = moved_order_id;
      orders_by_id_[moved_order_id].order_index = location.order_index;
    }

    level.orders.pop_back();
    level.order_count = static_cast<uint32_t>(level.orders.size());
    level.total_volume -= location.volume;

    if (level.empty()) {
      clear_active(location.side, location.level_index);
      if (location.side == Side::Bid && best_bid_index_ == location.level_index) {
        best_bid_index_ = find_best_bid_index();
      } else if (location.side == Side::Ask && best_ask_index_ == location.level_index) {
        best_ask_index_ = find_best_ask_index();
      }
    }

    orders_by_id_.erase(order_id);
  }

  void OrderBookV2_Vector::add_order(
      dt::OrderId order_id,
      dt::Price price,
      dt::Quantity volume,
      Side side) {
    if (volume == 0 || orders_by_id_.contains(order_id)) {
      return;
    }

    uint32_t level_index = 0;
    if (!price_to_index(price, level_index)) {
      return;
    }

    VectorPriceLevel& level = level_for(side, level_index);
    const bool was_empty = level.empty();
    const uint32_t order_index = static_cast<uint32_t>(level.orders.size());

    level.orders.push_back(order_id);
    level.total_volume += volume;
    level.order_count = static_cast<uint32_t>(level.orders.size());

    orders_by_id_[order_id] = {price, volume, side, level_index, order_index};

    if (was_empty) {
      set_active(side, level_index);
    }

    if (side == Side::Bid) {
      if (best_bid_index_ == INVALID_INDEX || level_index > best_bid_index_) {
        best_bid_index_ = level_index;
      }
    } else if (best_ask_index_ == INVALID_INDEX || level_index < best_ask_index_) {
      best_ask_index_ = level_index;
    }
  }

  void OrderBookV2_Vector::remove_order(dt::OrderId order_id) {
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
      return;
    }

    remove_order_at(order_id, it->second);
  }

  void OrderBookV2_Vector::modify_order(
      dt::OrderId order_id,
      dt::Price new_price,
      dt::Quantity new_volume) {
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
      return;
    }

    if (new_volume == 0) {
      remove_order(order_id);
      return;
    }

    VectorOrderLocation location = it->second;
    uint32_t new_level_index = 0;
    if (!price_to_index(new_price, new_level_index)) {
      return;
    }

    if (location.price == new_price) {
      VectorPriceLevel& level = level_for(location.side, location.level_index);
      level.total_volume -= location.volume;
      level.total_volume += new_volume;
      it->second.volume = new_volume;
      return;
    }

    const Side side = location.side;
    remove_order_at(order_id, location);
    add_order(order_id, new_price, new_volume, side);
  }

  void OrderBookV2_Vector::subtract_order(
      dt::OrderId order_id,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end() || volume == 0) {
      return;
    }

    VectorOrderLocation& location = it->second;
    if (volume >= location.volume) {
      remove_order(order_id);
      return;
    }

    VectorPriceLevel& level = level_for(location.side, location.level_index);
    level.total_volume -= volume;
    location.volume -= volume;
  }

  void OrderBookV2_Vector::replace_order(
      dt::OrderId original_order_id,
      dt::OrderId new_order_id,
      dt::Price price,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(original_order_id);
    if (it == orders_by_id_.end() || orders_by_id_.contains(new_order_id)) {
      return;
    }

    uint32_t level_index = 0;
    if (!price_to_index(price, level_index)) {
      return;
    }

    const Side side = it->second.side;
    remove_order(original_order_id);
    add_order(new_order_id, price, volume, side);
  }

  void OrderBookV2_Vector::print_top_levels(size_t depth) const {
    std::cout << "BIDS (best to worst)\n";
    std::size_t count = 0;
    for (uint32_t i = static_cast<uint32_t>(bid_levels_.size()); i > 0 && count < depth;) {
      --i;
      if (is_active(Side::Bid, i)) {
        std::cout << "Price " << index_to_price(i)
                  << ": volume=" << bid_levels_[i].volume() << std::endl;
        ++count;
      }
    }

    std::cout << "ASKS (best to worst)\n";
    count = 0;
    for (uint32_t i = 0; i < ask_levels_.size() && count < depth; ++i) {
      if (is_active(Side::Ask, i)) {
        std::cout << "Price " << index_to_price(i)
                  << ": volume=" << ask_levels_[i].volume() << std::endl;
        ++count;
      }
    }
  }

  sim::TopOfBook OrderBookV2_Vector::get_snapshot() const {
    sim::TopOfBook tob;
    if (best_bid_index_ != INVALID_INDEX) {
      const VectorPriceLevel& level = bid_levels_[best_bid_index_];
      tob.best_bid = index_to_price(best_bid_index_);
      tob.best_bid_qty = level.volume();
    }
    if (best_ask_index_ != INVALID_INDEX) {
      const VectorPriceLevel& level = ask_levels_[best_ask_index_];
      tob.best_ask = index_to_price(best_ask_index_);
      tob.best_ask_qty = level.volume();
    }
    return tob;
  }
} // namespace ob
