//
// Created by silay on 5/28/26.
//

#ifndef HFT_SIMULATOR_ORDER_BOOK_MICROBENCHMARK_HPP
#define HFT_SIMULATOR_ORDER_BOOK_MICROBENCHMARK_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Include nlohmann/json BEFORE entering benchmark namespace to avoid std::numeric_limits collision
#include <nlohmann/json.hpp>

#include "benchmark/common/benchmark.hpp"
#include "order_book/order_book_v1.hpp"
#include "order_book/order_book_v2_page_table.hpp"
#include "order_book/order_book_v2_vector.hpp"
#include "order_book/order_book_v2_hybrid.hpp"

namespace benchmark {
    namespace order_book {
        namespace microbenchmark {
            // Bring std types into scope since nlohmann/json may be parsed with benchmark::std active
            using std::array;
            using std::ostream;
            using std::string;
            using std::vector;

            // ========== CHANGE THIS TO SWITCH ORDER BOOK IMPLEMENTATIONS ==========
            using Book_impl = ob::OrderBookV1;
            //using Book_impl = ob::OrderBookV2_PageTable;
            //using Book_impl = ob::OrderBookV2_Vector;
            //using Book_impl = ob::OrderBookV2_Hybrid;
            // =====================================================================

            struct OrderBookSyntheticConfig {
                size_t initial_orders = 10000;
                size_t batch_size = 1000;
                size_t num_batches = 10;
                uint32_t min_price = 100000;
                uint32_t max_price = 500000;
                int32_t min_volume = 1;
                int32_t max_volume = 10000;
                uint64_t base_order_id = 1;
            };

            struct OrderBookOperationResult {
                double seconds = 0.0;
                double operations_per_second = 0.0;
                size_t operations = 0;
            };

            struct OrderBookOperationLatencyResult {
                static constexpr size_t MAX_CYCLES = benchmark::LatencyResult::MAX_CYCLES;

                std::array<uint64_t, MAX_CYCLES> histogram{};
                size_t operations = 0;

                uint64_t p50_cycles = 0;
                uint64_t p99_cycles = 0;
                uint64_t p999_cycles = 0;
                uint64_t max_cycles = 0;
                double mean_cycles = 0.0;
            };

            NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
                OrderBookOperationResult,
                operations,
                seconds,
                operations_per_second
            )

            NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
                OrderBookOperationLatencyResult,
                operations,
                p50_cycles,
                p99_cycles,
                p999_cycles,
                max_cycles,
                mean_cycles
            )

            // Throughput benchmarks
            OrderBookOperationResult run_add_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationResult run_cancel_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationResult run_execute_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationResult run_delete_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationResult run_modify_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            // Latency benchmarks
            OrderBookOperationLatencyResult run_add_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationLatencyResult run_cancel_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationLatencyResult run_execute_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationLatencyResult run_delete_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            OrderBookOperationLatencyResult run_modify_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
            );

            void print_result(const OrderBookOperationResult &result);

            void print_result(const OrderBookOperationLatencyResult &result, const std::string &hist_path);

            void write_json_result(const OrderBookOperationResult &result, std::ostream &out);

            void write_json_result(const OrderBookOperationLatencyResult &result, std::ostream &out);
        }
    }
}

#endif // HFT_SIMULATOR_ORDER_BOOK_MICROBENCHMARK_HPP
