//
// Created by silay on 5/26/26.
//

#include "benchmark/order_book/order_book_benchmark.hpp"

#include <iostream>

#include "benchmark/common/histogram.hpp"
#include "benchmark/common/rdtsc.hpp"
#include "market_data/itch_parser.hpp"
#include "order_book/order_book_v1.hpp"
#include "order_book/order_book_v2_hybrid.hpp"
#include "order_book/order_book_v2_page_table.hpp"
#include "order_book/order_book_v2_vector.hpp"

namespace benchmark {
    namespace order_book {
        OrderBookBenchmarkResult run_throughput(
            md::ITCHParser &parser,
            OrderBookITCHHandler &handler,
            const std::vector<char> &data
        ) {
            md::ParseResult parse_result{};

            auto benchmark_result =
                    benchmark::run_throughput([&] {
                        parse_result = parser.parse(data.data(), data.size(), handler);
                    });

            //do_not_optimize(parse_result);

            OrderBookBenchmarkResult result{};
            result.messages = parse_result.messages;
            result.skipped = parse_result.skipped;
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

            result.histogram.fill(0);

            while (cursor.is_in_bounds(1)) {
                const uint64_t start = benchmark::rdtsc_start();
                md::ParseOneResult parse_result = parser.parse_one(cursor, handler);
                const uint64_t end = benchmark::rdtsc_end();

                if (parse_result.needs_more_data) [[unlikely]] {
                    std::cerr << "Need more data, breaking latency benchmark\n";
                    break;
                }

                if (parse_result.parsed) {
                    ++result.messages;
                    const uint64_t cycles = end - start;
                    const uint64_t bucket = cycles < OrderBookLatencyBenchmarkResult::MAX_CYCLES
                                                ? cycles
                                                : OrderBookLatencyBenchmarkResult::MAX_CYCLES - 1;
                    ++result.histogram[bucket];
                } else if (parse_result.skipped) {
                    ++result.skipped;
                }
            }

            result.checksum = 0;
            result.p50_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.50);
            result.p99_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.99);
            result.p999_cycles = benchmark::histogram_percentile(result.histogram, result.messages, 0.999);

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

            benchmark::save_histogram(
                result.histogram,
                hist_path
            );

            std::cout << result_json.dump(4) << std::endl;
        }
    }
}
