//
// Created by silay on 5/26/26.
//

#include "benchmark/common/json_utilities.hpp"
#include "benchmark/parser/parser_benchmark.hpp"
#include <iostream>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <x86intrin.h>

#include "common/common.hpp"
#include "market_data/itch_parser.hpp"
#include <nlohmann/json.hpp>

#include "benchmark/common/histogram.hpp"

using json = nlohmann::json;

namespace benchmark::parser {
    ParserBenchmarkResult run_throughput(md::ITCHParser &parser, md::ITCHHandler &handler,
                                         const std::vector<char> &data) {
        parser.reset();
        handler.checksum = 0;

        md::ParseResult parse_result{};

        auto benchmark_result =
                benchmark::run_throughput([&] {
                    parse_result =
                            parser.parse(
                                data.data(),
                                data.size(),
                                handler
                            );
                });

        do_not_optimize(parse_result);

        ParserBenchmarkResult result{};

        result.messages =
                parse_result.messages;

        result.skipped =
                parse_result.skipped;

        result.checksum =
                handler.checksum;

        result.seconds =
                benchmark_result.seconds;

        result.messages_per_second =
                static_cast<double>(result.messages) /
                result.seconds;

        result.gibps =
                static_cast<double>(data.size()) /
                1024.0 / 1024.0 / 1024.0 /
                result.seconds;

        return result;
    }

    ParserLatencyBenchmarkResult run_latency(
        md::ITCHParser &parser,
        md::ITCHHandler &handler,
        const std::vector<char> &data
    ) {
        parser.reset();
        handler.checksum = 0;

        md::Cursor cursor(data.data(), data.size());

        ParserLatencyBenchmarkResult result{};

        const auto benchmark_result =
                benchmark::run_latency([&]() -> bool {
                    if (!cursor.is_in_bounds(1)) [[unlikely]] {
                        return true;
                    }

                    md::ParseOneResult parse_result = parser.parse_one(cursor, handler);

                    if (parse_result.needs_more_data) [[unlikely]]{
                        std::cerr << "Need more data, breaking latency benchmark\n";
                        return true;
                    }

                    if (parse_result.parsed) {
                        ++result.messages;
                    } else if (parse_result.skipped) {
                        ++result.skipped;
                    }

                    return false;
                });

        result.histogram = benchmark_result.histogram;

        result.checksum = handler.checksum;

        return result;
    }

    void print_result(const ParserBenchmarkResult &result) {
        json result_json = result;

        std::cout << result_json.dump(4) << std::endl;
    }

    void print_result(
        const ParserLatencyBenchmarkResult &result,
        const std::string &hist_path
    ) {
        json result_json = result;

        auto stats = benchmark::compute_latency_stats(result.histogram);

        result_json["p50_cycles"] = stats.p50_cycles;
        result_json["p99_cycles"] = stats.p99_cycles;
        result_json["p999_cycles"] = stats.p999_cycles;

        if (!hist_path.empty()) {
            benchmark::save_histogram(result.histogram, hist_path);
        }

        std::cout << result_json.dump(4) << std::endl;
    }

    std::string ParserBenchmarkResult::to_string() const {
        return
                "Messages: " + std::to_string(messages) + '\n' +
                "Skipped: " + std::to_string(skipped) + '\n' +
                "Checksum: " + std::to_string(checksum) + '\n' +
                "Seconds: " + std::to_string(seconds) + '\n' +
                "Messages per second: " + std::to_string(messages_per_second) + '\n' +
                "Gibps: " + std::to_string(gibps) + '\n';
    }

    std::string ParserLatencyBenchmarkResult::to_string() const {
        return {};
    }
}
