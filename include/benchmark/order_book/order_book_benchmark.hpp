/**
 * @file order_book_benchmark.hpp
 * @brief ITCH handler and benchmark functions for order books
 *
 * ## Design
 *
 * OrderBookITCHHandler wraps an OrderBook implementation and implements
 * the md::ITCHHandler interface. Since OrderBook base class already provides
 * ITCH handlers that forward to the public API, this just needs to:
 * 1. Expose an OrderBook (any implementation)
 * 2. Override ITCHHandler methods to call the corresponding OrderBook::on_* methods
 *
 * ## Usage
 *
 * To benchmark a different implementation, change the typedef:
 * ```cpp
 * using OB_impl = ob::OrderBookV2_PageTable;  // or V2_Vector, V2_Hybrid, etc.
 * ```
 */
#ifndef HFT_SIMULATOR_ORDER_BOOK_BENCHMARK_HPP
#define HFT_SIMULATOR_ORDER_BOOK_BENCHMARK_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "benchmark/common/benchmark.hpp"
#include "market_data/itch_parser.hpp"
#include "order_book/order_book_v1.hpp"
#include <nlohmann/json.hpp>

#include "order_book/order_book_v2_page_table.hpp"
#include "order_book/order_book_v2_vector.hpp"
#include "order_book/order_book_v2_hybrid.hpp"

namespace benchmark {
    namespace order_book {
        struct OrderBookITCHHandler;

        // Default to V1 - change this to benchmark other implementations
        using OB_impl = ob::OrderBookV1;
        //using OB_impl = ob::OrderBookV2_PageTable;
        //using OB_impl = ob::OrderBookV2_Vector;
        //using OB_impl = ob::OrderBookV2_Hybrid;

        struct OrderBookBenchmarkResult {
            double seconds = 0.0;
            double messages_per_second = 0.0;
            size_t messages = 0;
            size_t skipped = 0;
            uint64_t checksum = 0;
        };

        struct OrderBookLatencyBenchmarkResult {
            static constexpr size_t MAX_CYCLES = benchmark::LatencyResult::MAX_CYCLES;

            std::array<uint64_t, MAX_CYCLES> histogram{};
            size_t messages = 0;
            size_t skipped = 0;
            uint64_t checksum = 0;

            uint64_t p50_cycles = 0;
            uint64_t p99_cycles = 0;
            uint64_t p999_cycles = 0;
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
            OrderBookBenchmarkResult,
            messages,
            skipped,
            checksum,
            seconds,
            messages_per_second
        )

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
            OrderBookLatencyBenchmarkResult,
            messages,
            skipped,
            checksum,
            p50_cycles,
            p99_cycles,
            p999_cycles
        )

        OrderBookBenchmarkResult run_throughput(
            md::ITCHParser &parser,
            OrderBookITCHHandler &handler,
            const std::vector<char> &data
        );

        OrderBookLatencyBenchmarkResult run_latency(
            md::ITCHParser &parser,
            OrderBookITCHHandler &handler,
            const std::vector<char> &data
        );

        void print_result(const OrderBookBenchmarkResult &result);

        void print_result(const OrderBookLatencyBenchmarkResult &result, const std::string &hist_path);

        /**
         * @brief ITCH handler that forwards to OrderBook
         *
         * Simply calls the corresponding on_* method on the wrapped OrderBook.
         * Since OrderBook base class implements these to forward to the public API,
         * this just needs to delegate.
         */
        struct OrderBookITCHHandler : public md::ITCHHandler {
            OB_impl order_book; // The order book implementation to test

            void on_add_order(uint16_t locate, uint16_t tracking, uint64_t timestamp, uint64_t order_ref,
                              char side, uint32_t shares, uint64_t stock, uint32_t price) override {
                order_book.on_add_order(locate, tracking, timestamp, order_ref, side, shares, stock, price);
            }

            void on_add_order_with_mpid(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                        uint64_t order_ref, char side, uint32_t shares,
                                        uint64_t stock, uint32_t price, uint32_t mpid) override {
                order_book.on_add_order_with_mpid(locate, tracking, timestamp, order_ref, side, shares, stock, price,
                                                  mpid);
            }

            void on_order_executed(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                   uint64_t order_ref, uint32_t executed_shares, uint64_t match_id) override {
                order_book.on_order_executed(locate, tracking, timestamp, order_ref, executed_shares, match_id);
            }

            void on_order_executed_with_price(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                              uint64_t order_ref, uint32_t executed_shares,
                                              uint64_t match_id, char printable, uint32_t price) override {
                order_book.on_order_executed_with_price(locate, tracking, timestamp, order_ref, executed_shares,
                                                        match_id, printable, price);
            }

            void on_order_cancel(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref, uint32_t canceled_shares) override {
                order_book.on_order_cancel(locate, tracking, timestamp, order_ref, canceled_shares);
            }

            void on_order_delete(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref) override {
                order_book.on_order_delete(locate, tracking, timestamp, order_ref);
            }

            void on_order_replace(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                  uint64_t original_order_ref, uint64_t new_order_ref,
                                  uint32_t shares, uint32_t price) override {
                order_book.on_order_replace(locate, tracking, timestamp, original_order_ref, new_order_ref, shares,
                                            price);
            }
        };
    }
} // namespace benchmark::order_book

#endif // HFT_SIMULATOR_ORDER_BOOK_BENCHMARK_HPP
