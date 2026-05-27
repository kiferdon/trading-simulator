//
// Created by silay on 5/26/26.
//

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/parser/parser_benchmark.hpp"
#include "common/common.hpp"
#include "market_data/itch_parser.hpp"

int main(int argc, char **argv) {
    std::cout << "Parser throughput benchmark" << std::endl;

    const int runs = argc > 1 ? std::stoi(argv[1]) : 1;
    const std::string data_path = argc > 2
                                      ? argv[2]
                                      : "../market-data/12302019.NASDAQ_ITCH50";
    const std::string json_path = argc > 3
                                      ? argv[3]
                                      : "../benchmarks/saved/output.json";

    std::cout << "Runs: " << runs << std::endl;

    if (runs < 1) {
        std::cerr << "Run count must be positive\n";
        return 0;
    }

    std::vector<char> data = common::read_file(data_path);

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
            return benchmark::parser::run_throughput(
                parser,
                handler,
                data
            );
        },
        [&](const benchmark::parser::ParserBenchmarkResult &result) {
            benchmark::parser::print_result(result);
            benchmark::write_json_result(result, json_file);
        }
    );
}
