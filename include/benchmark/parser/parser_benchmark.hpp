//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_PARSER_BENCHMARK_HPP
#define HFT_SIMULATOR_PARSER_BENCHMARK_HPP
#include <cstdint>
#include <vector>

#include "benchmark/common/benchmark.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace md {
    struct ITCHHandler;
    class ITCHParser;
}

namespace benchmark::parser {
    struct ParserBenchmarkResult {
        double seconds_before = 0.0;
        double seconds_after = 0.0;

        double seconds = 0.0;
        double nanoseconds = 0.0;

        double messages_per_second = 0.0;
        double gibps = 0.0;

        size_t messages = 0;
        size_t skipped = 0;

        uint64_t checksum = 0;

        std::string to_string() const;
    };

    struct ParserLatencyBenchmarkResult {
        static constexpr size_t MAX_CYCLES = benchmark::LatencyResult::MAX_CYCLES;

        std::array<uint64_t, MAX_CYCLES> histogram{};

        size_t messages = 0;
        size_t skipped = 0;

        uint64_t checksum = 0;

        uint64_t p50_cycles = 0;
        uint64_t p99_cycles = 0;
        uint64_t p999_cycles = 0;

        std::string to_string() const;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        ParserBenchmarkResult,
        messages,
        skipped,
        checksum,
        seconds,
        messages_per_second,
        gibps
    )

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        ParserLatencyBenchmarkResult,
        messages,
        skipped,
        checksum,
        p50_cycles,
        p99_cycles,
        p999_cycles
    )

    ParserBenchmarkResult run_throughput(md::ITCHParser &parser, md::ITCHHandler &handler,
                                         const std::vector<char> &data);

    ParserLatencyBenchmarkResult run_latency(
        md::ITCHParser &parser,
        md::ITCHHandler &handler,
        const std::vector<char> &data);

    int run_parse_benchmark(const std::vector<char> &data, bool runWarmup);

    int run_benchmark_main(int argc, char **argv);

    void print_result(const ParserBenchmarkResult &result);

    void print_result(const ParserLatencyBenchmarkResult &result, const std::string &hist_path);
}

#endif //HFT_SIMULATOR_PARSER_BENCHMARK_HPP
