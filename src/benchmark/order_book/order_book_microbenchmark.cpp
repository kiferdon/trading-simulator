//
// Created by silay on 5/28/26.
//

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "benchmark/common/histogram.hpp"
#include "benchmark/order_book/order_book_microbenchmark.hpp"

namespace benchmark {
    namespace order_book {
        namespace microbenchmark {
            namespace {
                uint32_t lcg_next(uint32_t &state) {
                    state = state * 1103515245 + 12345;
                    return (state >> 16) & 0x7FFF;
                }

                uint32_t lcg_range(uint32_t &state, uint32_t min_val, uint32_t max_val) {
                    if (min_val >= max_val) return min_val;
                    return min_val + (lcg_next(state) % (max_val - min_val));
                }

                struct SyntheticOrder {
                    dt::OrderId order_id;
                    dt::Price price;
                    dt::Quantity volume;
                    ob::Side side;
                };

                std::vector<SyntheticOrder> generate_orders(
                    uint32_t seed,
                    size_t count,
                    uint32_t min_price,
                    uint32_t max_price,
                    int32_t min_volume,
                    int32_t max_volume,
                    uint64_t start_order_id
                ) {
                    std::vector<SyntheticOrder> orders;
                    orders.reserve(count);

                    uint32_t rng_state = seed;

                    for (size_t i = 0; i < count; ++i) {
                        SyntheticOrder order;
                        order.order_id = start_order_id + i;
                        order.price = lcg_range(rng_state, min_price, max_price);
                        order.volume = lcg_range(rng_state,
                                                 static_cast<uint32_t>(min_volume),
                                                 static_cast<uint32_t>(max_volume));
                        order.side = (lcg_next(rng_state) & 1) ? ob::Side::Bid : ob::Side::Ask;
                        orders.push_back(order);
                    }

                    return orders;
                }
            }

            // ============== Throughput Benchmarks ==============

            OrderBookOperationResult run_add_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                const size_t operations = config.batch_size * config.num_batches;

                auto orders = generate_orders(
                    42,
                    operations,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_throughput([&] {
                    for (const auto &order: orders) {
                        order_book.add_order(order.order_id, order.price, order.volume, order.side);
                    }
                });

                OrderBookOperationResult result{};
                result.operations = operations;
                result.seconds = bench_result.seconds;
                if (result.seconds > 0.0) {
                    result.operations_per_second = static_cast<double>(operations) / result.seconds;
                }

                return result;
            }

            OrderBookOperationResult run_cancel_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                for (const auto &order: orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                const size_t cancel_count = std::min(
                    config.batch_size,
                    static_cast<size_t>(config.initial_orders)
                );

                auto cancel_orders = generate_orders(
                    43,
                    cancel_count,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    1,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_throughput([&] {
                    for (size_t i = 0; i < cancel_orders.size(); ++i) {
                        order_book.subtract_order(cancel_orders[i].order_id, cancel_orders[i].volume);
                    }
                });

                OrderBookOperationResult result{};
                result.operations = cancel_count;
                result.seconds = bench_result.seconds;
                if (result.seconds > 0.0) {
                    result.operations_per_second = static_cast<double>(cancel_count) / result.seconds;
                }

                return result;
            }

            OrderBookOperationResult run_execute_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                return run_cancel_orders(order_book, config);
            }

            OrderBookOperationResult run_delete_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                for (const auto &order: orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                const size_t delete_count = std::min(
                    config.batch_size,
                    static_cast<size_t>(config.initial_orders)
                );

                auto delete_orders = generate_orders(
                    43,
                    delete_count,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_throughput([&] {
                    for (const auto &order: delete_orders) {
                        order_book.remove_order(order.order_id);
                    }
                });

                OrderBookOperationResult result{};
                result.operations = delete_count;
                result.seconds = bench_result.seconds;
                if (result.seconds > 0.0) {
                    result.operations_per_second = static_cast<double>(delete_count) / result.seconds;
                }

                return result;
            }

            OrderBookOperationResult run_modify_orders(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                for (const auto &order: orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                const size_t modify_count = std::min(
                    config.batch_size,
                    static_cast<size_t>(config.initial_orders)
                );

                auto modify_orders = generate_orders(
                    43,
                    modify_count,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_throughput([&] {
                    for (const auto &order: modify_orders) {
                        dt::Price new_price = static_cast<dt::Price>((order.price * 101) >> 8);
                        dt::Quantity new_volume = static_cast<dt::Quantity>((order.volume * 3) >> 1);
                        order_book.modify_order(order.order_id, new_price, new_volume);
                    }
                });

                OrderBookOperationResult result{};
                result.operations = modify_count;
                result.seconds = bench_result.seconds;
                if (result.seconds > 0.0) {
                    result.operations_per_second = static_cast<double>(modify_count) / result.seconds;
                }

                return result;
            }

            // ============== Latency Benchmarks ==============

            OrderBookOperationLatencyResult run_add_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto orders = generate_orders(
                    42,
                    config.batch_size,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                size_t order_index = 0;
                auto bench_result = benchmark::run_latency([&]() -> bool {
                    if (order_index >= orders.size()) {
                        return true;
                    }
                    const auto& order = orders[order_index++];
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                    return false;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                auto stats = benchmark::compute_latency_stats(result.histogram);
                result.operations = stats.total_samples;
                result.max_cycles = stats.max_cycles;
                result.mean_cycles = stats.mean_cycles;
                result.p50_cycles = stats.p50_cycles;
                result.p99_cycles = stats.p99_cycles;
                result.p999_cycles = stats.p999_cycles;

                return result;
            }

            OrderBookOperationLatencyResult run_cancel_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                const size_t batch_count = std::min(config.batch_size, config.initial_orders);

                auto setup_orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );
                for (const auto& order : setup_orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                auto cancel_orders = setup_orders;
                size_t order_index = 0;

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    if (order_index >= cancel_orders.size()) {
                        return true;
                    }
                    const auto& order = cancel_orders[order_index++];
                    order_book.subtract_order(order.order_id, order.volume);
                    return false;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                auto stats = benchmark::compute_latency_stats(result.histogram);
                result.operations = stats.total_samples;
                result.max_cycles = stats.max_cycles;
                result.mean_cycles = stats.mean_cycles;
                result.p50_cycles = stats.p50_cycles;
                result.p99_cycles = stats.p99_cycles;
                result.p999_cycles = stats.p999_cycles;

                return result;
            }

            OrderBookOperationLatencyResult run_execute_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                return run_cancel_orders_latency(order_book, config);
            }

            OrderBookOperationLatencyResult run_delete_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto setup_orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );
                for (const auto& order : setup_orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                auto delete_orders = setup_orders;
                size_t order_index = 0;

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    if (order_index >= delete_orders.size()) {
                        return true;
                    }
                    const auto& order = delete_orders[order_index++];
                    order_book.remove_order(order.order_id);
                    return false;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                auto stats = benchmark::compute_latency_stats(result.histogram);
                result.operations = stats.total_samples;
                result.max_cycles = stats.max_cycles;
                result.mean_cycles = stats.mean_cycles;
                result.p50_cycles = stats.p50_cycles;
                result.p99_cycles = stats.p99_cycles;
                result.p999_cycles = stats.p999_cycles;

                return result;
            }

            OrderBookOperationLatencyResult run_modify_orders_latency(
                Book_impl &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto setup_orders = generate_orders(
                    42,
                    config.initial_orders,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );
                for (const auto& order : setup_orders) {
                    order_book.add_order(order.order_id, order.price, order.volume, order.side);
                }

                auto modify_orders = setup_orders;
                size_t order_index = 0;

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    if (order_index >= modify_orders.size()) {
                        return true;
                    }
                    const auto& order = modify_orders[order_index++];
                    dt::Price new_price = static_cast<dt::Price>((order.price * 101) >> 8);
                    dt::Quantity new_volume = static_cast<dt::Quantity>((order.volume * 3) >> 1);
                    order_book.modify_order(order.order_id, new_price, new_volume);
                    return false;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                auto stats = benchmark::compute_latency_stats(result.histogram);
                result.operations = stats.total_samples;
                result.max_cycles = stats.max_cycles;
                result.mean_cycles = stats.mean_cycles;
                result.p50_cycles = stats.p50_cycles;
                result.p99_cycles = stats.p99_cycles;
                result.p999_cycles = stats.p999_cycles;

                return result;
            }

            // ============== JSON/Print Functions ==============

            void write_json_result(const OrderBookOperationResult &result, std::ostream &out) {
                nlohmann::json j = result;
                out << j.dump() << "\n";
            }

            void write_json_result(const OrderBookOperationLatencyResult &result, std::ostream &out) {
                nlohmann::json j = result;
                out << j.dump() << "\n";
            }

            void print_result(const OrderBookOperationResult &result) {
                nlohmann::json result_json = result;
                std::cout << result_json.dump(4) << std::endl;
            }

            void print_result(const OrderBookOperationLatencyResult &result, const std::string &hist_path) {
                nlohmann::json result_json = result;
                if (!hist_path.empty()) {
                    benchmark::save_histogram(result.histogram, hist_path);
                }
                std::cout << result_json.dump(4) << std::endl;
            }
        }
    }
}