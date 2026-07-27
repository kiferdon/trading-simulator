//
// Created by silay on 6/6/26.
//

#ifndef HFT_SIMULATOR_BENCHMARK_OUTPUT_HPP
#define HFT_SIMULATOR_BENCHMARK_OUTPUT_HPP

#include <fstream>
#include <string>

namespace benchmark {
    // Derives the histogram output directory from a JSON file path.
    // Extracts the directory component (everything up to the last '/').
    inline std::string derive_output_dir(const std::string &json_path) {
        size_t pos = json_path.find_last_of("/\\");
        if (pos == std::string::npos) {
            return ".";  // No directory component
        }
        return json_path.substr(0, pos);
    }

    // Manages output file paths and streams for benchmark results.
    // - JSONL output stream (compact one object per line)
    // - Histogram file path derivation from name suffix
    class BenchmarkOutput {
    public:
        // json_path: path to the JSONL output file
        explicit BenchmarkOutput(const std::string &json_path)
            : json_file_(json_path)
            , hist_dir_(derive_output_dir(json_path)) {}

        std::ostream &json() { return json_file_; }

        // Builds a histogram file path: <hist_dir>/<name>_latency.hist
        std::string hist_path(const char *name) const {
            return hist_dir_ + "/" + std::string(name) + "_latency.hist";
        }

    private:
        std::ofstream json_file_;
        std::string hist_dir_;
    };
}

#endif //HFT_SIMULATOR_BENCHMARK_OUTPUT_HPP