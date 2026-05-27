//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_HISTOGRAM_HPP
#define HFT_SIMULATOR_HISTOGRAM_HPP
#include <fstream>
#include <string>

namespace benchmark {
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
}

#endif //HFT_SIMULATOR_HISTOGRAM_HPP
