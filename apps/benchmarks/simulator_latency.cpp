//
// Simulator latency benchmark.
//

#include <cstdlib>
#include <iostream>
#include <string>

#include "benchmark/common/benchmark.hpp"
#include "benchmark/common/json_utilities.hpp"
#include "benchmark/common/output.hpp"
#include "benchmark/simulator/simulator_benchmark.hpp"
#include "common/common.hpp"

int main(int argc, char **argv) {
    std::cout << "Simulator latency benchmark" << std::endl;

    const int runs = argc > 1 ? std::atoi(argv[1]) : 1;
    const std::string data_path = argc > 2
                                      ? argv[2]
                                      : "../market-data/12302019.NASDAQ_ITCH50";
    const std::string json_path = argc > 3
                                      ? argv[3]
                                      : "../benchmarks/saved/output.jsonl";

    if (runs < 1) {
        std::cerr << "Run count must be positive\n";
        return 1;
    }

    std::cout << "Runs: " << runs << std::endl;

    const std::vector<char> data = common::read_file(data_path);

    benchmark::BenchmarkSessionConfig config;
    config.runs = runs;

    benchmark::BenchmarkOutput output(json_path);

    benchmark::run_benchmark_session(
        config,
        [] {},
        [&] {
            return benchmark::simulator::run_latency(data);
        },
        [&](const auto &result) {
            benchmark::simulator::print_result(result, output.hist_path("simulator"));
            benchmark::write_json_result(result, output.json());
        }
    );

    return 0;
}
