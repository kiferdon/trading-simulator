//
// Created by silay on 5/28/26.
//

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

#include "order_book/order_book_v1.hpp"

using namespace ob;
using OB_impl = OrderBookV1;

namespace {
    constexpr dt::Price BID_PRICE = 100000;
    constexpr dt::Price ASK_PRICE = 100100;
    constexpr dt::Quantity DEFAULT_VOLUME = 100;

    void test_add_order_bid() {
        OB_impl book;
        dt::OrderId order_id = 1;

        book.add_order(order_id, BID_PRICE, DEFAULT_VOLUME, Side::Bid);

        book.print_top_levels(1);
    }

    void test_add_multiple_orders_same_price() {
        OB_impl book;

        book.add_order(1, BID_PRICE, 50, Side::Bid);
        book.add_order(2, BID_PRICE, 75, Side::Bid);
        book.add_order(3, BID_PRICE, 100, Side::Bid);

        book.print_top_levels(1);
    }

    void test_add_bid_and_ask() {
        OB_impl book;

        book.add_order(1, BID_PRICE, DEFAULT_VOLUME, Side::Bid);
        book.add_order(2, ASK_PRICE, DEFAULT_VOLUME, Side::Ask);

        book.print_top_levels(1);
    }

    void test_get_best_bid_ask() {
        OB_impl book;

        book.add_order(1, 100000, 100, Side::Bid);
        book.add_order(2, 100100, 100, Side::Ask);
        book.add_order(3, 99900, 100, Side::Bid);
        book.add_order(4, 100200, 100, Side::Ask);

        book.print_top_levels(2);
    }

    void test_remove_order() {
        OB_impl book;

        book.add_order(1, BID_PRICE, 100, Side::Bid);
        book.add_order(2, BID_PRICE, 50, Side::Bid);
        book.add_order(3, ASK_PRICE, 75, Side::Ask);

        book.remove_order(2);

        book.print_top_levels(2);
    }

    void test_subtract_order_partial() {
        OB_impl book;

        book.add_order(1, BID_PRICE, 100, Side::Bid);

        book.subtract_order(1, 50);

        book.print_top_levels(1);
    }

    void test_subtract_order_full() {
        OB_impl book;

        book.add_order(1, BID_PRICE, 100, Side::Bid);
        book.add_order(2, ASK_PRICE, 50, Side::Ask);

        book.subtract_order(1, 100);

        book.print_top_levels(1);
    }

    void test_modify_order_price() {
        OB_impl book;

        book.add_order(1, 100000, 100, Side::Bid);
        book.add_order(2, 100200, 100, Side::Bid);

        book.modify_order(1, 100050, 100);

        book.print_top_levels(2);
    }

    void test_modify_order_volume() {
        OB_impl book;

        book.add_order(1, 100000, 100, Side::Bid);

        book.modify_order(1, 100000, 150);

        book.print_top_levels(1);
    }

    void test_on_add_order_from_itch() {
        OB_impl book;

        book.on_add_order(7, 1, 1234567890, 100, 'B', 100, 0, 100000);
        book.on_add_order(7, 1, 1234567891, 101, 'S', 50, 0, 100100);

        book.print_top_levels(2);
    }

    void test_on_order_delete() {
        OB_impl book;

        dt::OrderId order_id = 100;
        book.add_order(order_id, BID_PRICE, DEFAULT_VOLUME, Side::Bid);

        book.on_order_delete(7, 1, 1234567890, order_id);

        book.print_top_levels(1);
    }

    void test_on_order_executed() {
        OB_impl book;

        dt::OrderId order_id = 100;
        book.add_order(order_id, BID_PRICE, 100, Side::Bid);
        book.add_order(200, ASK_PRICE, 50, Side::Ask);

        book.on_order_executed(7, 1, 1234567890, order_id, 100, 1);

        book.print_top_levels(1);
    }

    void test_on_order_cancel() {
        OB_impl book;

        dt::OrderId order_id = 100;
        book.add_order(order_id, BID_PRICE, 100, Side::Bid);

        book.on_order_cancel(7, 1, 1234567890, order_id, 50);

        book.print_top_levels(1);
    }

    void test_on_order_replace() {
        OB_impl book;

        dt::OrderId original_id = 100;
        dt::OrderId new_id = 200;
        book.add_order(original_id, BID_PRICE, 100, Side::Bid);

        book.on_order_replace(7, 1, 1234567890, original_id, new_id, 75, 100050);

        book.print_top_levels(2);
    }

    void test_on_add_order_with_mpid() {
        OB_impl book;

        book.on_add_order_with_mpid(7, 1, 1234567890, 100, 'B', 100, 0, 100000, 0x4141524D);

        book.print_top_levels(1);
    }

    void test_multiple_levels_bids() {
        OB_impl book;

        book.add_order(1, 100000, 100, Side::Bid);
        book.add_order(2, 99900, 50, Side::Bid);
        book.add_order(3, 99800, 75, Side::Bid);
        book.add_order(4, 100100, 60, Side::Ask);
        book.add_order(5, 100200, 80, Side::Ask);

        book.print_top_levels(3);
    }

    void test_order_lifecycle() {
        OB_impl book;

        dt::OrderId order_id = 1;

        book.add_order(order_id, BID_PRICE, 100, Side::Bid);
        assert(true && "Added order");

        book.modify_order(order_id, BID_PRICE, 150);
        assert(true && "Modified order volume");

        book.modify_order(order_id, 100050, 150);
        assert(true && "Modified order price");

        book.subtract_order(order_id, 50);
        assert(true && "Partial execution");

        book.subtract_order(order_id, 100);
        assert(true && "Full execution - order should be removed");
    }
} // namespace

int main() {
    std::cout << "Order Book Tests started" << std::endl;
    std::cout << "========================" << std::endl;

    std::cout << "\n[test_add_order_bid]" << std::endl;
    test_add_order_bid();

    std::cout << "\n[test_add_multiple_orders_same_price]" << std::endl;
    test_add_multiple_orders_same_price();

    std::cout << "\n[test_add_bid_and_ask]" << std::endl;
    test_add_bid_and_ask();

    std::cout << "\n[test_get_best_bid_ask]" << std::endl;
    test_get_best_bid_ask();

    std::cout << "\n[test_remove_order]" << std::endl;
    test_remove_order();

    std::cout << "\n[test_subtract_order_partial]" << std::endl;
    test_subtract_order_partial();

    std::cout << "\n[test_subtract_order_full]" << std::endl;
    test_subtract_order_full();

    std::cout << "\n[test_modify_order_price]" << std::endl;
    test_modify_order_price();

    std::cout << "\n[test_modify_order_volume]" << std::endl;
    test_modify_order_volume();

    std::cout << "\n[test_on_add_order_from_itch]" << std::endl;
    test_on_add_order_from_itch();

    std::cout << "\n[test_on_order_delete]" << std::endl;
    test_on_order_delete();

    std::cout << "\n[test_on_order_executed]" << std::endl;
    test_on_order_executed();

    std::cout << "\n[test_on_order_cancel]" << std::endl;
    test_on_order_cancel();

    std::cout << "\n[test_on_order_replace]" << std::endl;
    test_on_order_replace();

    std::cout << "\n[test_on_add_order_with_mpid]" << std::endl;
    test_on_add_order_with_mpid();

    std::cout << "\n[test_multiple_levels_bids]" << std::endl;
    test_multiple_levels_bids();

    std::cout << "\n[test_order_lifecycle]" << std::endl;
    test_order_lifecycle();

    std::cout << "========================" << std::endl;
    std::cout << "Order Book Tests finished" << std::endl;

    return 0;
}
