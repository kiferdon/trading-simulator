//
// Created by silay on 5/28/26.
//

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/order_book/order_book_benchmark.hpp"
#include "common/common.hpp"
#include "market_data/itch_parser.hpp"
#include "order_book/order_book_v1.hpp"

int main(int argc, char **argv) {
    std::cout << "Order book benchmark" << std::endl;

    const int runs = argc > 1 ? std::atoi(argv[1]) : 1;
    const std::string data_path = argc > 2
                                      ? argv[2]
                                      : "../market-data/12302019.NASDAQ_ITCH50";
    const std::string json_path = argc > 3
                                      ? argv[3]
                                      : "../benchmarks/saved/output.json";
    const std::string hist_path = argc > 4
                                      ? argv[4]
                                      : "../benchmarks/saved/hist.txt";

    std::cout << "Runs: " << runs << std::endl;

    if (runs < 1) {
        std::cerr << "Run count must be positive\n";
        return 1;
    }

    std::vector<char> data = common::read_file(data_path);

    md::ITCHParser parser;
    benchmark::order_book::OrderBookITCHHandler handler;

    benchmark::BenchmarkSessionConfig config;
    config.runs = runs;

    std::ofstream json_file(json_path);

    std::cout << "\n=== Throughput ===" << std::endl;
    benchmark::run_benchmark_session(
        config,
        [] {
        },
        [&] {
            return benchmark::order_book::run_throughput(
                parser,
                handler,
                data
            );
        },
        [&](const auto &result) {
            benchmark::order_book::print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );

    std::cout << "\n=== Latency ===" << std::endl;
    benchmark::run_benchmark_session(
        config,
        [] {
        },
        [&] {
            return benchmark::order_book::run_latency(
                parser,
                handler,
                data
            );
        },
        [&](const auto &result) {
            benchmark::order_book::print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );

    return 0;
}
