//
// Created by silay on 5/28/26.
//

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/order_book/order_book_microbenchmark.hpp"
#include "order_book/order_book_v1.hpp"

int main(int argc, char **argv) {
    std::cout << "Order book microbenchmark" << std::endl;

    const int runs = argc > 1 ? std::atoi(argv[1]) : 1;
    const std::string json_path = argc > 2
                                      ? argv[2]
                                      : "../benchmarks/saved/output.json";
    const std::string hist_path = argc > 3
                                       ? argv[3]
                                       : "../benchmarks/saved/hist.txt";

    using namespace benchmark::order_book::microbenchmark;

    OrderBookSyntheticConfig config{};
    config.initial_orders = 10000;
    config.batch_size = 1000;
    config.num_batches = 10;
    config.min_price = 100000;
    config.max_price = 500000;
    config.min_volume = 1;
    config.max_volume = 10000;
    config.base_order_id = 1;

    benchmark::BenchmarkSessionConfig session_config{};
    session_config.runs = runs;

    std::ofstream json_file(json_path);

    std::cout << "\n=== Add Orders (throughput) ===" << std::endl;
    ob::OrderBookV1 book_for_add;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_add_orders(book_for_add, config);
        },
        [&](const auto &result) {
            print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Add Orders (latency) ===" << std::endl;
    ob::OrderBookV1 book_for_add_latency;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_add_orders_latency(book_for_add_latency, config);
        },
        [&](const auto &result) {
            print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Cancel Orders (throughput) ===" << std::endl;
    ob::OrderBookV1 book_for_cancel;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_cancel_orders(book_for_cancel, config);
        },
        [&](const auto &result) {
            print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Cancel Orders (latency) ===" << std::endl;
    ob::OrderBookV1 book_for_cancel_latency;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_cancel_orders_latency(book_for_cancel_latency, config);
        },
        [&](const auto &result) {
            print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Delete Orders (throughput) ===" << std::endl;
    ob::OrderBookV1 book_for_delete;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_delete_orders(book_for_delete, config);
        },
        [&](const auto &result) {
            print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Delete Orders (latency) ===" << std::endl;
    ob::OrderBookV1 book_for_delete_latency;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_delete_orders_latency(book_for_delete_latency, config);
        },
        [&](const auto &result) {
            print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Modify Orders (throughput) ===" << std::endl;
    ob::OrderBookV1 book_for_modify;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_modify_orders(book_for_modify, config);
        },
        [&](const auto &result) {
            print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Modify Orders (latency) ===" << std::endl;
    ob::OrderBookV1 book_for_modify_latency;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            return run_modify_orders_latency(book_for_modify_latency, config);
        },
        [&](const auto &result) {
            print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );

    return 0;
}