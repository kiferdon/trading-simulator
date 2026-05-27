//
// Created by silay on 5/26/26.
//

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/parser/parser_benchmark.hpp"
#include "common/common.hpp"
#include "market_data/itch_parser.hpp"

int main(int argc, char **argv) {
    std::cout << "Parser latency benchmark" << std::endl;

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
        return 0;
    }

    auto data = common::read_file(data_path);

    md::ITCHParser parser;
    md::ITCHHandler handler;

    benchmark::BenchmarkSessionConfig config;
    config.runs = runs;

    std::ofstream json_file(json_path);

    benchmark::run_benchmark_session(
        config,
        [] {
        },
        [&] {
            return benchmark::parser::run_latency(
                parser,
                handler,
                data
            );
        },
        [&, hist_path](const auto &result) {
            benchmark::parser::print_result(result, hist_path);
            benchmark::write_json_result(result, json_file);
        }
    );
}
