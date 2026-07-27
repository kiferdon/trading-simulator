//
// OrderBookV2_PageTable implementation
//
#include "order_book/order_book_v2_page_table.hpp"
#include <iostream>
#include <algorithm>

namespace ob {
  // PriceLevel methods
  void PriceLevel::push_front(IntrusiveOrderNode* node) {
    if (head == nullptr) {
      head = node;
      tail = node;
      node->next = nullptr;
      node->prev = nullptr;
    } else {
      node->next = head;
      node->prev = nullptr;
      head->prev = node;
      head = node;
    }
    ++order_count;
    total_volume += node->volume;
  }

  void PriceLevel::remove(IntrusiveOrderNode* node) {
    if (node->prev) {
      node->prev->next = node->next;
    } else {
      head = node->next;
    }

    if (node->next) {
      node->next->prev = node->prev;
    } else {
      tail = node->prev;
    }

    if (order_count > 0) --order_count;
    total_volume = (total_volume > node->volume) ? total_volume - node->volume : 0;

    node->next = nullptr;
    node->prev = nullptr;
  }

  // OrderBookV2_PageTable methods
  PriceLevel& OrderBookV2_PageTable::get_level(dt::Price price, Side side) {
    size_t page_index = price >> PAGE_BITS;
    dt::Price page_offset = price & PRICE_MASK;

    if (side == Side::Bid) {
      if (page_index >= bid_pages_.size()) {
        bid_pages_.resize(page_index + 1, nullptr);
      }
      if (bid_pages_[page_index] == nullptr) {
        bid_pages_[page_index] = new PriceLevel[PAGE_SIZE]();
      }
      return bid_pages_[page_index][page_offset];
    } else {
      if (page_index >= ask_pages_.size()) {
        ask_pages_.resize(page_index + 1, nullptr);
      }
      if (ask_pages_[page_index] == nullptr) {
        ask_pages_[page_index] = new PriceLevel[PAGE_SIZE]();
      }
      return ask_pages_[page_index][page_offset];
    }
  }

  const PriceLevel* OrderBookV2_PageTable::find_level(dt::Price price, Side side) const {
    const size_t page_index = price >> PAGE_BITS;
    const dt::Price page_offset = price & PRICE_MASK;
    const auto& pages = side == Side::Bid ? bid_pages_ : ask_pages_;
    if (page_index >= pages.size() || pages[page_index] == nullptr) {
      return nullptr;
    }
    return &pages[page_index][page_offset];
  }

  dt::Price OrderBookV2_PageTable::find_best_bid() const {
    for (size_t page_index = bid_pages_.size(); page_index > 0;) {
      --page_index;
      PriceLevel* page = bid_pages_[page_index];
      if (page == nullptr) {
        continue;
      }

      for (dt::Price offset = PAGE_SIZE; offset > 0;) {
        --offset;
        if (!page[offset].empty()) {
          return static_cast<dt::Price>((page_index << PAGE_BITS) + offset);
        }
      }
    }
    return 0;
  }

  dt::Price OrderBookV2_PageTable::find_best_ask() const {
    for (size_t page_index = 0; page_index < ask_pages_.size(); ++page_index) {
      PriceLevel* page = ask_pages_[page_index];
      if (page == nullptr) {
        continue;
      }

      for (dt::Price offset = 0; offset < PAGE_SIZE; ++offset) {
        if (!page[offset].empty()) {
          return static_cast<dt::Price>((page_index << PAGE_BITS) + offset);
        }
      }
    }
    return std::numeric_limits<dt::Price>::max();
  }

  void OrderBookV2_PageTable::refresh_best_after_empty(Side side, dt::Price price) {
    if (side == Side::Bid && price == best_bid_) {
      best_bid_ = find_best_bid();
    } else if (side == Side::Ask && price == best_ask_) {
      best_ask_ = find_best_ask();
    }
  }

  void OrderBookV2_PageTable::add_order(
      dt::OrderId order_id,
      dt::Price price,
      dt::Quantity volume,
      Side side) {
    if (volume == 0 || orders_by_id_.contains(order_id)) {
      return;
    }

    // Create the order node
    order_pool_.emplace_back(order_id, price, volume, side);
    IntrusiveOrderNode* node = &order_pool_.back();

    // Get the price level and add the node
    PriceLevel& level = get_level(price, side);
    level.push_front(node);

    // Store in lookup map
    orders_by_id_[order_id] = node;

    // Update best prices
    if (side == Side::Bid && price > best_bid_) {
      best_bid_ = price;
    } else if (side == Side::Ask && price < best_ask_) {
      best_ask_ = price;
    }
  }

  void OrderBookV2_PageTable::remove_order(dt::OrderId order_id) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end()) {
      IntrusiveOrderNode* node = it->second;
      const Side side = node->side;
      const dt::Price price = node->price;
      PriceLevel& level = get_level(node->price, node->side);
      level.remove(node);
      orders_by_id_.erase(order_id);
      if (level.empty()) {
        refresh_best_after_empty(side, price);
      }
    }
  }

  void OrderBookV2_PageTable::modify_order(
      dt::OrderId order_id,
      dt::Price new_price,
      dt::Quantity new_volume) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end()) {
      if (new_volume == 0) {
        remove_order(order_id);
        return;
      }

      IntrusiveOrderNode* node = it->second;
      if (node->price == new_price) {
        // Only volume changed - update in place
        PriceLevel& level = get_level(node->price, node->side);
        level.total_volume -= node->volume;
        node->volume = new_volume;
        level.total_volume += node->volume;
      } else {
        // Price changed - remove and re-add
        Side side = node->side;
        remove_order(order_id);
        add_order(order_id, new_price, new_volume, side);
      }
    }
  }

  void OrderBookV2_PageTable::subtract_order(
      dt::OrderId order_id,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end() && volume > 0) {
      IntrusiveOrderNode* node = it->second;
      if (volume >= node->volume) {
        // Full removal
        remove_order(order_id);
      } else {
        // Partial reduction
        node->volume -= volume;
        PriceLevel& level = get_level(node->price, node->side);
        level.total_volume -= volume;
      }
    }
  }

  void OrderBookV2_PageTable::replace_order(
      dt::OrderId original_order_id,
      dt::OrderId new_order_id,
      dt::Price price,
      dt::Quantity volume) {
    auto it = orders_by_id_.find(original_order_id);
    if (it != orders_by_id_.end() && !orders_by_id_.contains(new_order_id)) {
      Side side = it->second->side;
      remove_order(original_order_id);
      add_order(new_order_id, price, volume, side);
    }
  }

  void OrderBookV2_PageTable::print_top_levels(size_t depth) const {
    std::cout << "BIDS (best to worst)\n";
    std::size_t count = 0;
    for (size_t page_index = bid_pages_.size(); page_index > 0 && count < depth;) {
      --page_index;
      PriceLevel* page = bid_pages_[page_index];
      if (page == nullptr) {
        continue;
      }
      for (dt::Price offset = PAGE_SIZE; offset > 0 && count < depth;) {
        --offset;
        if (!page[offset].empty()) {
          std::cout << "Price " << static_cast<dt::Price>((page_index << PAGE_BITS) + offset)
                    << ": volume=" << page[offset].volume() << std::endl;
          ++count;
        }
      }
    }

    std::cout << "ASKS (best to worst)\n";
    count = 0;
    for (size_t page_index = 0; page_index < ask_pages_.size() && count < depth; ++page_index) {
      PriceLevel* page = ask_pages_[page_index];
      if (page == nullptr) {
        continue;
      }
      for (dt::Price offset = 0; offset < PAGE_SIZE && count < depth; ++offset) {
        if (!page[offset].empty()) {
          std::cout << "Price " << static_cast<dt::Price>((page_index << PAGE_BITS) + offset)
                    << ": volume=" << page[offset].volume() << std::endl;
          ++count;
        }
      }
    }
  }

  sim::TopOfBook OrderBookV2_PageTable::get_snapshot() const {
    sim::TopOfBook tob;
    if (best_bid_ != 0) {
      if (const PriceLevel* level = find_level(best_bid_, Side::Bid); level != nullptr) {
        tob.best_bid = best_bid_;
        tob.best_bid_qty = level->volume();
      }
    }
    if (best_ask_ != std::numeric_limits<dt::Price>::max()) {
      if (const PriceLevel* level = find_level(best_ask_, Side::Ask); level != nullptr) {
        tob.best_ask = best_ask_;
        tob.best_ask_qty = level->volume();
      }
    }
    return tob;
  }
} // namespace ob
