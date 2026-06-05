//
// Created by Claude on 2026-06-04.
//

#include "simulator/market.hpp"
#include "simulator/simulator.hpp"
#include "order_book/order_book_v1.hpp"

void Market::add_tracked_stock(dt::StockLocate locate) {
    if (tracked_stocks_.insert(locate).second) {
        order_books_[locate] = std::make_unique<ob::OrderBookV1>();
    }
}

ob::OrderBook* Market::get_order_book(dt::StockLocate locate) {
    auto it = order_books_.find(locate);
    return (it != order_books_.end()) ? it->second.get() : nullptr;
}

void Market::set_event_handler(sim::Simulator* handler) {
    event_handler_ = handler;
}

void Market::set_event_throttle(std::optional<dt::StockLocate> locate, uint32_t every_n_events) {
    if (locate) {
        if (auto* book = get_order_book(*locate)) {
            book->set_event_throttle(every_n_events);
        }
    } else {
        for (auto& [_, book] : order_books_) {
            book->set_event_throttle(every_n_events);
        }
    }
}

void Market::on_add_order(uint16_t locate, uint16_t, uint64_t timestamp,
                           uint64_t order_ref, char side, uint32_t shares,
                           uint64_t, uint32_t price) {
    if (auto* book = get_order_book(locate)) {
        book->add_order(order_ref, price, shares, ob::itch_side_to_order_book(side));
    }
    on_market_event(timestamp, locate);
}

void Market::on_add_order_with_mpid(uint16_t locate, uint16_t, uint64_t timestamp,
                                    uint64_t order_ref, char side, uint32_t shares,
                                    uint64_t, uint32_t price, uint32_t) {
    if (auto* book = get_order_book(locate)) {
        book->add_order(order_ref, price, shares, ob::itch_side_to_order_book(side));
    }
    on_market_event(timestamp, locate);
}

void Market::on_order_executed(uint16_t locate, uint16_t, uint64_t timestamp,
                                uint64_t order_ref, uint32_t executed_shares, uint64_t) {
    if (auto* book = get_order_book(locate)) {
        book->subtract_order(order_ref, executed_shares);
    }
    on_market_event(timestamp, locate);
}

void Market::on_order_executed_with_price(uint16_t locate, uint16_t, uint64_t timestamp,
                                          uint64_t order_ref, uint32_t executed_shares,
                                          uint64_t, char, uint32_t) {
    if (auto* book = get_order_book(locate)) {
        book->subtract_order(order_ref, executed_shares);
    }
    on_market_event(timestamp, locate);
}

void Market::on_order_cancel(uint16_t locate, uint16_t, uint64_t timestamp,
                              uint64_t order_ref, uint32_t canceled_shares) {
    if (auto* book = get_order_book(locate)) {
        book->subtract_order(order_ref, canceled_shares);
    }
    on_market_event(timestamp, locate);
}

void Market::on_order_delete(uint16_t locate, uint16_t, uint64_t timestamp,
                               uint64_t order_ref) {
    if (auto* book = get_order_book(locate)) {
        book->remove_order(order_ref);
    }
    on_market_event(timestamp, locate);
}

void Market::on_order_replace(uint16_t locate, uint16_t, uint64_t timestamp,
                               uint64_t original_order_ref, uint64_t new_order_ref,
                               uint32_t shares, uint32_t price) {
    if (auto* book = get_order_book(locate)) {
        book->replace_order(original_order_ref, new_order_ref, price, shares);
    }
    on_market_event(timestamp, locate);
}

void Market::on_market_event(uint64_t timestamp, uint16_t locate) {
    auto* book = get_order_book(locate);
    if (!book) {
        return;
    }

    book->increment_event_counter();

    if (book->should_emit()) {
        sim::MarketEvent event{};
        event.timestamp = timestamp;
        event.stock_locate = locate;
        event.snapshot = book->get_snapshot();
        if (event_handler_) {
            event_handler_->on_market_event(event);
        }
    }
}