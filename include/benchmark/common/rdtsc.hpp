//
// Created by silay on 5/23/26.
//

#ifndef BENCHMARK_COMMON_RDTSC_HPP
#define BENCHMARK_COMMON_RDTSC_HPP
#include <cstdint>
#include <emmintrin.h>

namespace benchmark {
    static inline uint64_t rdtsc_start() {
        _mm_lfence(); // serialize before
        return __rdtsc();
    }

    static inline uint64_t rdtsc_end() {
        unsigned int aux;
        const uint64_t t = __rdtscp(&aux);
        _mm_lfence();
        return t;
    }
}

#endif //BENCHMARK_COMMON_RDTSC_HPP
