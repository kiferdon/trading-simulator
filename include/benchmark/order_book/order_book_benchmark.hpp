//
// Created by silay on 5/26/26.
//

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

namespace benchmark {
    namespace order_book {
        struct OrderBookITCHHandler;
        using OB_impl = ob::OrderBookV1;

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

        struct OrderBookITCHHandler : public md::ITCHHandler {
            virtual void on_add_order(uint16_t locate, uint16_t tracking, uint64_t timestamp, uint64_t order_ref,
                                      char side, uint32_t shares, uint64_t stock, uint32_t price) override {
                order_book.add_order(order_ref, price, shares, ob::itch_side_to_order_book(side));
            }

            void on_add_order_with_mpid(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                        uint64_t order_ref, char side, uint32_t shares,
                                        uint64_t stock, uint32_t price, uint32_t mpid) {
                order_book.add_order(order_ref, price, shares, ob::itch_side_to_order_book(side));
            }

            void on_order_executed(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                   uint64_t order_ref, uint32_t executed_shares, uint64_t match_id) {
            }

            void on_order_executed_with_price(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                              uint64_t order_ref, uint32_t executed_shares,
                                              uint64_t match_id, char printable, uint32_t price) {
                order_book.subtract_order(order_ref, executed_shares);
            }

            void on_order_cancel(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref, uint32_t canceled_shares) {
                order_book.subtract_order(order_ref, canceled_shares);
            }

            void on_order_delete(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref) {
                order_book.remove_order(order_ref);
            }

            void on_order_replace(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                  uint64_t original_order_ref, uint64_t new_order_ref,
                                  uint32_t shares, uint32_t price) {
                order_book.replace_order(original_order_ref, new_order_ref, shares, price);
            }


            ob::OrderBook order_book;
        };
    }
} // namespace benchmark::order_book

#endif // HFT_SIMULATOR_ORDER_BOOK_BENCHMARK_HPP
