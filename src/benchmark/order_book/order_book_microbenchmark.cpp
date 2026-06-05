//
// Created by silay on 5/28/26.
//

#include "benchmark/order_book/order_book_microbenchmark.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "benchmark/common/benchmark.hpp"

namespace benchmark {
    template<typename Histogram>
    uint64_t histogram_percentile(
        const Histogram &histogram,
        uint64_t total_samples,
        double percentile
    ) {
        const uint64_t target = static_cast<uint64_t>(
            static_cast<double>(total_samples) * percentile
        );

        uint64_t cumulative = 0;

        for (size_t bucket = 0; bucket < histogram.size(); ++bucket) {
            cumulative += histogram[bucket];

            if (cumulative >= target) {
                return bucket;
            }
        }

        return 0;
    }

    template<typename Histogram>
    void save_histogram_file(
        const Histogram &histogram,
        const std::string &path
    ) {
        std::ofstream out_file(path);
        for (size_t bucket = 0; bucket < histogram.size(); ++bucket) {
            out_file << bucket << ' ' << histogram[bucket] << '\n';
        }
    }
}

namespace benchmark {
    namespace order_book {
        namespace microbenchmark {
            namespace {
                uint32_t lcg_next(uint32_t &state) {
                    state = state * 1103515245 + 12345;
                    return (state >> 16) & 0x7FFF;
                }

                uint32_t lcg_range(uint32_t &state, uint32_t min_val, uint32_t max_val) {
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

            OrderBookOperationResult run_add_orders(
                ob::OrderBookV1 &order_book,
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
                result.operations_per_second = static_cast<double>(operations) / result.seconds;

                return result;
            }

            OrderBookOperationLatencyResult run_add_orders_latency(
                ob::OrderBookV1 &order_book,
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

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    for (const auto &order: orders) {
                        order_book.add_order(order.order_id, order.price, order.volume, order.side);
                    }
                    return true;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                uint64_t total_cycles = 0;
                uint64_t max_cycles = 0;
                size_t total_samples = 0;

                for (size_t i = 0; i < result.histogram.size(); ++i) {
                    uint64_t count = result.histogram[i];
                    if (count > 0) {
                        total_cycles += i * count;
                        total_samples += count;
                        if (i > max_cycles) {
                            max_cycles = i;
                        }
                    }
                }

                result.operations = total_samples;
                result.max_cycles = max_cycles;

                if (total_samples > 0) {
                    result.mean_cycles = static_cast<double>(total_cycles) / static_cast<double>(total_samples);
                }

                result.p50_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.50);
                result.p99_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.99);
                result.p999_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.999);

                return result;
            }

            OrderBookOperationResult run_cancel_orders(
                ob::OrderBookV1 &order_book,
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
                    for (const auto &order: cancel_orders) {
                        order_book.subtract_order(order.order_id, order.volume);
                    }
                });

                OrderBookOperationResult result{};
                result.operations = cancel_count;
                result.seconds = bench_result.seconds;
                result.operations_per_second = static_cast<double>(cancel_count) / result.seconds;

                return result;
            }

            OrderBookOperationLatencyResult run_cancel_orders_latency(
                ob::OrderBookV1 &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto cancel_orders = generate_orders(
                    43,
                    config.batch_size,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    1,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    for (const auto &order: cancel_orders) {
                        order_book.subtract_order(order.order_id, order.volume);
                    }
                    return true;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                uint64_t total_cycles = 0;
                uint64_t max_cycles = 0;
                size_t total_samples = 0;

                for (size_t i = 0; i < result.histogram.size(); ++i) {
                    uint64_t count = result.histogram[i];
                    if (count > 0) {
                        total_cycles += i * count;
                        total_samples += count;
                        if (i > max_cycles) {
                            max_cycles = i;
                        }
                    }
                }

                result.operations = total_samples;
                result.max_cycles = max_cycles;

                if (total_samples > 0) {
                    result.mean_cycles = static_cast<double>(total_cycles) / static_cast<double>(total_samples);
                }

                result.p50_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.50);
                result.p99_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.99);
                result.p999_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.999);

                return result;
            }

            OrderBookOperationResult run_execute_orders(
                ob::OrderBookV1 &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                return run_cancel_orders(order_book, config);
            }

            OrderBookOperationLatencyResult run_execute_orders_latency(
                ob::OrderBookV1 &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                return run_cancel_orders_latency(order_book, config);
            }

            OrderBookOperationResult run_delete_orders(
                ob::OrderBookV1 &order_book,
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
                result.operations_per_second = static_cast<double>(delete_count) / result.seconds;

                return result;
            }

            OrderBookOperationLatencyResult run_delete_orders_latency(
                ob::OrderBookV1 &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto delete_orders = generate_orders(
                    43,
                    config.batch_size,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    for (const auto &order: delete_orders) {
                        order_book.remove_order(order.order_id);
                    }
                    return true;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                uint64_t total_cycles = 0;
                uint64_t max_cycles = 0;
                size_t total_samples = 0;

                for (size_t i = 0; i < result.histogram.size(); ++i) {
                    uint64_t count = result.histogram[i];
                    if (count > 0) {
                        total_cycles += i * count;
                        total_samples += count;
                        if (i > max_cycles) {
                            max_cycles = i;
                        }
                    }
                }

                result.operations = total_samples;
                result.max_cycles = max_cycles;

                if (total_samples > 0) {
                    result.mean_cycles = static_cast<double>(total_cycles) / static_cast<double>(total_samples);
                }

                result.p50_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.50);
                result.p99_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.99);
                result.p999_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.999);

                return result;
            }

            OrderBookOperationResult run_modify_orders(
                ob::OrderBookV1 &order_book,
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
                result.operations_per_second = static_cast<double>(modify_count) / result.seconds;

                return result;
            }

            OrderBookOperationLatencyResult run_modify_orders_latency(
                ob::OrderBookV1 &order_book,
                const OrderBookSyntheticConfig &config
            ) {
                auto modify_orders = generate_orders(
                    43,
                    config.batch_size,
                    config.min_price,
                    config.max_price,
                    config.min_volume,
                    config.max_volume,
                    config.base_order_id
                );

                auto bench_result = benchmark::run_latency([&]() -> bool {
                    for (const auto &order: modify_orders) {
                        dt::Price new_price = static_cast<dt::Price>((order.price * 101) >> 8);
                        dt::Quantity new_volume = static_cast<dt::Quantity>((order.volume * 3) >> 1);
                        order_book.modify_order(order.order_id, new_price, new_volume);
                    }
                    return true;
                });

                OrderBookOperationLatencyResult result{};
                result.histogram = bench_result.histogram;

                uint64_t total_cycles = 0;
                uint64_t max_cycles = 0;
                size_t total_samples = 0;

                for (size_t i = 0; i < result.histogram.size(); ++i) {
                    uint64_t count = result.histogram[i];
                    if (count > 0) {
                        total_cycles += i * count;
                        total_samples += count;
                        if (i > max_cycles) {
                            max_cycles = i;
                        }
                    }
                }

                result.operations = total_samples;
                result.max_cycles = max_cycles;

                if (total_samples > 0) {
                    result.mean_cycles = static_cast<double>(total_cycles) / static_cast<double>(total_samples);
                }

                result.p50_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.50);
                result.p99_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.99);
                result.p999_cycles = benchmark::histogram_percentile(result.histogram, total_samples, 0.999);

                return result;
            }

            void print_result(const OrderBookOperationResult &result) {
                nlohmann::json result_json = result;
                std::cout << result_json.dump(4) << std::endl;
            }

            void print_result(const OrderBookOperationLatencyResult &result, const std::string &hist_path) {
                nlohmann::json result_json = result;
                benchmark::save_histogram_file(result.histogram, hist_path);
                std::cout << result_json.dump(4) << std::endl;
            }
        }
    }
}
