//
// Created by silay on 5/26/26.
//

#include "benchmark/order_book/order_book_benchmark.hpp"

#include <iostream>

#include "benchmark/common/histogram.hpp"
#include "market_data/itch_parser.hpp"
#include "order_book/order_book_v1.hpp"

namespace benchmark {
    namespace order_book {
        OrderBookBenchmarkResult run_throughput(
            md::ITCHParser &parser,
            OrderBookITCHHandler &handler,
            const std::vector<char> &data
        ) {
            md::Cursor cursor(data.data(), data.size());
            md::ParseOneResult parse_result{};
            size_t messages = 0;
            size_t skipped = 0;

            auto benchmark_result =
                    benchmark::run_throughput([&] {
                        cursor.reset();

                        while (!cursor.is_in_bounds(1)) [[unlikely]] {
                        }

                        while (true) {
                            parse_result = parser.parse_one(cursor, handler);

                            if (parse_result.needs_more_data) [[unlikely]] {
                                break;
                            }

                            if (parse_result.parsed) {
                                ++messages;
                            } else if (parse_result.skipped) {
                                ++skipped;
                            }

                            if (!cursor.is_in_bounds(1)) {
                                break;
                            }
                        }
                    });

            OrderBookBenchmarkResult result{};
            result.messages = messages;
            result.skipped = skipped;
            result.checksum = 0;
            result.seconds = benchmark_result.seconds;
            result.messages_per_second =
                    static_cast<double>(result.messages) /
                    result.seconds;

            return result;
        }

        OrderBookLatencyBenchmarkResult run_latency(
            md::ITCHParser &parser,
            OrderBookITCHHandler &handler,
            const std::vector<char> &data
        ) {
            md::Cursor cursor(data.data(), data.size());

            OrderBookLatencyBenchmarkResult result{};

            const auto benchmark_result =
                    benchmark::run_latency([&]() -> bool {
                        if (!cursor.is_in_bounds(1)) [[unlikely]] {
                            return true;
                        }

                        md::ParseOneResult parse_result = parser.parse_one(cursor, handler);

                        if (parse_result.needs_more_data) [[unlikely]] {
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
            result.checksum = 0;

            return result;
        }

        void print_result(const OrderBookBenchmarkResult &result) {
            nlohmann::json result_json = result;
            std::cout << result_json.dump(4) << std::endl;
        }

        void print_result(
            const OrderBookLatencyBenchmarkResult &result,
            const std::string &hist_path
        ) {
            nlohmann::json result_json = result;

            result_json["p50_cycles"] =
                    benchmark::histogram_percentile(
                        result.histogram,
                        result.messages,
                        0.50
                    );

            result_json["p99_cycles"] =
                    benchmark::histogram_percentile(
                        result.histogram,
                        result.messages,
                        0.99
                    );

            result_json["p999_cycles"] =
                    benchmark::histogram_percentile(
                        result.histogram,
                        result.messages,
                        0.999
                    );

            benchmark::save_histogram(
                result.histogram,
                hist_path
            );

            std::cout << result_json.dump(4) << std::endl;
        }
    }
}
