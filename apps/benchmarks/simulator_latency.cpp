//
// Created by Claude on 2026-05-29.
//

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "benchmark/simulator/simulator_benchmark.hpp"

void load_file(const std::string &path, std::vector<char> &data) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(size);
    if (!file.read(data.data(), size)) {
        throw std::runtime_error("Failed to read file");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <itch_file>" << std::endl;
        return 1;
    }

    const std::string path = argv[1];

    try {
        std::vector<char> data;
        load_file(path, data);
        std::cout << "Loaded " << data.size() << " bytes" << std::endl;

        benchmark::simulator::SimulatorLatencyBenchmarkResult result;
        result = benchmark::simulator::run_latency(data);

        benchmark::simulator::print_result(result, "../benchmarks/saved/simulator_latency_hist.txt");
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}