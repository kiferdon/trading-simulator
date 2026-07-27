//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_HISTOGRAM_HPP
#define HFT_SIMULATOR_HISTOGRAM_HPP

#include <cstdint>
#include <fstream>
#include <string>

namespace benchmark {
    struct LatencyStats {
        uint64_t p50_cycles = 0;
        uint64_t p99_cycles = 0;
        uint64_t p999_cycles = 0;
        uint64_t max_cycles = 0;
        double mean_cycles = 0.0;
        size_t total_samples = 0;
    };

    template<typename Histogram>
    uint64_t histogram_percentile(
        const Histogram &histogram,
        uint64_t total_samples,
        double percentile
    ) {
        const uint64_t target =
                static_cast<uint64_t>(
                    static_cast<double>(total_samples) * percentile
                );

        uint64_t cumulative = 0;

        for (size_t bucket = 0;
             bucket < histogram.size();
             ++bucket) {
            cumulative += histogram[bucket];

            if (cumulative >= target) {
                return bucket;
            }
        }

        return 0;
    }

    template<typename Histogram>
    void save_histogram(
        const Histogram &histogram,
        const std::string &path
    ) {
        std::ofstream out_file(path);

        for (size_t bucket = 0; bucket < histogram.size(); ++bucket) {
            out_file << bucket << ' ' << histogram[bucket] << '\n';
        }
    }

    template<typename Histogram>
    LatencyStats compute_latency_stats(const Histogram &histogram) {
        LatencyStats stats;

        uint64_t total_cycles = 0;

        for (size_t i = 0; i < histogram.size(); ++i) {
            uint64_t count = histogram[i];
            if (count > 0) {
                stats.total_samples += count;
                total_cycles += i * count;
                if (i > stats.max_cycles) {
                    stats.max_cycles = i;
                }
            }
        }

        if (stats.total_samples > 0) {
            stats.mean_cycles = static_cast<double>(total_cycles) /
                                 static_cast<double>(stats.total_samples);
        }

        stats.p50_cycles  = histogram_percentile(histogram, stats.total_samples, 0.50);
        stats.p99_cycles  = histogram_percentile(histogram, stats.total_samples, 0.99);
        stats.p999_cycles = histogram_percentile(histogram, stats.total_samples, 0.999);

        return stats;
    }
}

#endif //HFT_SIMULATOR_HISTOGRAM_HPP