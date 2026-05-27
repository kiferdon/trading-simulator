//
// Created by silay on 5/26/26.
//

#include "benchmark/common/benchmark.hpp"
#include <nlohmann/json.hpp>

void benchmark::benchmark_warmup(size_t runs) {
    std::cout << "Running warmup with " << runs << " iterations" << std::endl;
    uint64_t result = 0;

    for (size_t i = 0; i < runs; ++i) {
        result += i;
    }

    asm volatile("" : : "r,m"(result) : "memory");
    std::cout << "Warmup completed" << std::endl;
}
