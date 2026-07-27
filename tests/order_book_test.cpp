//
// Correctness tests for all OrderBook implementations.
//
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

#include "order_book/order_book.hpp"
#include "order_book/order_book_v1.hpp"
#include "order_book/order_book_v2_hybrid.hpp"
#include "order_book/order_book_v2_page_table.hpp"
#include "order_book/order_book_v2_vector.hpp"

using namespace ob;

namespace {
  constexpr dt::Price BID_PRICE = 100000;
  constexpr dt::Price BID_PRICE_LOWER = 99900;
  constexpr dt::Price BID_PRICE_HIGHER = 100050;
  constexpr dt::Price ASK_PRICE = 100100;
  constexpr dt::Price ASK_PRICE_BETTER = 100050;

  struct TestStats {
    uint32_t checks = 0;
    uint32_t failures = 0;
    std::ostream* report = &std::cout;

    void expect_eq(const std::string& impl_name,
                   const std::string& test_name,
                   const char* field,
                   uint32_t actual,
                   uint32_t expected) {
      ++checks;
      if (actual == expected) {
        return;
      }

      ++failures;
      *report << "[FAIL] " << impl_name << " :: " << test_name
              << " :: " << field << " expected=" << expected
              << " actual=" << actual << '\n';
    }
  };

  void expect_snapshot(TestStats& stats,
                       const std::string& impl_name,
                       const std::string& test_name,
                       const sim::TopOfBook& snapshot,
                       uint32_t best_bid,
                       uint32_t best_bid_qty,
                       uint32_t best_ask,
                       uint32_t best_ask_qty) {
    stats.expect_eq(impl_name, test_name, "best_bid", snapshot.best_bid, best_bid);
    stats.expect_eq(impl_name, test_name, "best_bid_qty", snapshot.best_bid_qty, best_bid_qty);
    stats.expect_eq(impl_name, test_name, "best_ask", snapshot.best_ask, best_ask);
    stats.expect_eq(impl_name, test_name, "best_ask_qty", snapshot.best_ask_qty, best_ask_qty);
  }

  template<typename OB>
  void test_add_bid_and_ask(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "add_bid_and_ask";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_aggregate_same_price(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "aggregate_same_price";
    OB book;

    book.add_order(1, BID_PRICE, 50, Side::Bid);
    book.add_order(2, BID_PRICE, 75, Side::Bid);
    book.add_order(3, BID_PRICE, 100, Side::Bid);
    book.add_order(4, ASK_PRICE, 25, Side::Ask);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 225, ASK_PRICE, 25);
  }

  template<typename OB>
  void test_snapshot_quantities(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "snapshot_quantities";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE, 25, Side::Bid);
    book.add_order(3, ASK_PRICE, 50, Side::Ask);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 125, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_partial_cancel_reduces_volume(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "partial_cancel_reduces_volume";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.subtract_order(1, 40);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 60, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_full_cancel_recomputes_best_bid(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "full_cancel_recomputes_best_bid";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE_LOWER, 50, Side::Bid);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.subtract_order(1, 100);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE_LOWER, 50, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_full_cancel_recomputes_best_ask(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "full_cancel_recomputes_best_ask";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE_BETTER, 50, Side::Ask);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.subtract_order(2, 50);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_over_cancel_removes_order(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "over_cancel_removes_order";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE_LOWER, 60, Side::Bid);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.subtract_order(1, 125);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE_LOWER, 60, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_delete_preserves_opposite_side(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "delete_preserves_opposite_side";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.remove_order(1);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    0, 0, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_replace_preserves_side(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "replace_preserves_side";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.replace_order(2, 3, ASK_PRICE_BETTER, 75);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE_BETTER, 75);
  }

  template<typename OB>
  void test_replace_preserves_ask_side(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "replace_preserves_ask_side";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.replace_order(2, 3, ASK_PRICE_BETTER, 75);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE_BETTER, 75);
  }

  template<typename OB>
  void test_modify_same_price_changes_volume(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "modify_same_price_changes_volume";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.modify_order(1, BID_PRICE, 150);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 150, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_modify_new_price_moves_level(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "modify_new_price_moves_level";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE_LOWER, 50, Side::Bid);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.modify_order(1, BID_PRICE_HIGHER, 120);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE_HIGHER, 120, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_same_page_prices_stay_distinct(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "same_page_prices_stay_distinct";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE_HIGHER, 50, Side::Bid);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.subtract_order(2, 50);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_same_page_prices_remain_distinct(TestStats& stats,
                                             const std::string& impl_name) {
    const std::string test_name = "same_page_prices_remain_distinct";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, BID_PRICE_HIGHER, 50, Side::Bid);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.remove_order(2);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_best_ask_recomputes_after_remove(TestStats& stats,
                                             const std::string& impl_name) {
    const std::string test_name = "best_ask_recomputes_after_remove";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE_BETTER, 50, Side::Ask);
    book.add_order(3, ASK_PRICE, 75, Side::Ask);
    book.remove_order(2);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 75);
  }

  template<typename OB>
  void test_itch_handlers(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "itch_handlers";
    OB book;

    book.on_add_order(7, 1, 1234567890, 1, 'B', 100, 0, BID_PRICE);
    book.on_add_order(7, 1, 1234567891, 2, 'S', 50, 0, ASK_PRICE);
    book.on_order_cancel(7, 1, 1234567892, 1, 25);
    book.on_order_replace(7, 1, 1234567893, 2, 3, 75, ASK_PRICE_BETTER);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 75, ASK_PRICE_BETTER, 75);
  }

  template<typename OB>
  void test_missing_order_operations_are_noops(TestStats& stats, const std::string& impl_name) {
    const std::string test_name = "missing_order_operations_are_noops";
    OB book;

    book.add_order(1, BID_PRICE, 100, Side::Bid);
    book.add_order(2, ASK_PRICE, 50, Side::Ask);
    book.remove_order(9001);
    book.subtract_order(9001, 25);
    book.modify_order(9001, BID_PRICE_HIGHER, 75);
    book.replace_order(9001, 9002, ASK_PRICE_BETTER, 80);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE, 100, ASK_PRICE, 50);
  }

  template<typename OB>
  void test_itch_delete_and_execute_with_price(TestStats& stats,
                                               const std::string& impl_name) {
    const std::string test_name = "itch_delete_and_execute_with_price";
    OB book;

    book.on_add_order(7, 1, 1234567890, 1, 'B', 100, 0, BID_PRICE);
    book.on_add_order(7, 1, 1234567891, 2, 'B', 80, 0, BID_PRICE_LOWER);
    book.on_add_order(7, 1, 1234567892, 3, 'S', 50, 0, ASK_PRICE_BETTER);
    book.on_add_order(7, 1, 1234567893, 4, 'S', 75, 0, ASK_PRICE);
    book.on_order_executed_with_price(7, 1, 1234567894, 1, 100, 0, 'N', BID_PRICE);
    book.on_order_delete(7, 1, 1234567895, 3);

    expect_snapshot(stats, impl_name, test_name, book.get_snapshot(),
                    BID_PRICE_LOWER, 80, ASK_PRICE, 75);
  }

  template<typename OB>
  void run_all_tests_for_impl(TestStats& stats, const std::string& impl_name) {
    *stats.report << "Testing " << impl_name << '\n';

    test_add_bid_and_ask<OB>(stats, impl_name);
    test_aggregate_same_price<OB>(stats, impl_name);
    test_snapshot_quantities<OB>(stats, impl_name);
    test_partial_cancel_reduces_volume<OB>(stats, impl_name);
    test_full_cancel_recomputes_best_bid<OB>(stats, impl_name);
    test_full_cancel_recomputes_best_ask<OB>(stats, impl_name);
    test_over_cancel_removes_order<OB>(stats, impl_name);
    test_delete_preserves_opposite_side<OB>(stats, impl_name);
    test_replace_preserves_side<OB>(stats, impl_name);
    test_replace_preserves_ask_side<OB>(stats, impl_name);
    test_modify_same_price_changes_volume<OB>(stats, impl_name);
    test_modify_new_price_moves_level<OB>(stats, impl_name);
    test_same_page_prices_stay_distinct<OB>(stats, impl_name);
    test_same_page_prices_remain_distinct<OB>(stats, impl_name);
    test_best_ask_recomputes_after_remove<OB>(stats, impl_name);
    test_itch_handlers<OB>(stats, impl_name);
    test_missing_order_operations_are_noops<OB>(stats, impl_name);
    test_itch_delete_and_execute_with_price<OB>(stats, impl_name);
  }
}

int main() {
  std::filesystem::create_directories("test-reports");
  std::ofstream report_file("test-reports/order_book_test_report.txt");

  TestStats stats;
  stats.report = &report_file;

  *stats.report << "Order book correctness report\n";
  *stats.report << "=============================\n";

  run_all_tests_for_impl<OrderBookV1>(stats, "OrderBookV1");
  run_all_tests_for_impl<OrderBookV2_PageTable>(stats, "OrderBookV2_PageTable");
  run_all_tests_for_impl<OrderBookV2_Vector>(stats, "OrderBookV2_Vector");
  run_all_tests_for_impl<OrderBookV2_Hybrid>(stats, "OrderBookV2_Hybrid");

  *stats.report << "Order book correctness checks: " << stats.checks
                << ", failures: " << stats.failures << '\n';

  report_file.close();

  std::ifstream report_in("test-reports/order_book_test_report.txt");
  std::cout << report_in.rdbuf();
  std::cout << "Report saved to test-reports/order_book_test_report.txt\n";

  return stats.failures == 0 ? 0 : 1;
}
