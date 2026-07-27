//
// Order book synthetic operation throughput microbenchmarks.
//

#include <cstdlib>
#include <iostream>
#include <string>

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/common/output.hpp"
#include "benchmark/order_book/order_book_microbenchmark.hpp"

int main(int argc, char **argv) {
    std::cout << "Order book throughput microbenchmark" << std::endl;

    const int runs = argc > 1 ? std::atoi(argv[1]) : 1;
    const std::string json_path = argc > 3 ? argv[3] : "../benchmarks/saved/output.jsonl";

    if (runs < 1) {
        std::cerr << "Run count must be positive\n";
        return 1;
    }

    benchmark::BenchmarkOutput output(json_path);

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

    auto write_result = [&](const auto& result) {
        benchmark::write_json_result(result, output.json());
    };

    std::cout << "\n=== Add Orders ===" << std::endl;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            Book_impl book_for_add;
            return run_add_orders(book_for_add, config);
        },
        [&](const auto &result) {
            print_result(result);
            write_result(result);
        }
    );

    std::cout << "\n=== Cancel Orders ===" << std::endl;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            Book_impl book_for_cancel;
            return run_cancel_orders(book_for_cancel, config);
        },
        [&](const auto &result) {
            print_result(result);
            write_result(result);
        }
    );

    std::cout << "\n=== Delete Orders ===" << std::endl;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            Book_impl book_for_delete;
            return run_delete_orders(book_for_delete, config);
        },
        [&](const auto &result) {
            print_result(result);
            write_result(result);
        }
    );

    std::cout << "\n=== Modify Orders ===" << std::endl;
    benchmark::run_benchmark_session(
        session_config,
        [] {},
        [&] {
            Book_impl book_for_modify;
            return run_modify_orders(book_for_modify, config);
        },
        [&](const auto &result) {
            print_result(result);
            write_result(result);
        }
    );

    return 0;
}
