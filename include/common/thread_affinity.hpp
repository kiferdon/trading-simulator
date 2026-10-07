#ifndef HFT_SIMULATOR_THREAD_AFFINITY_HPP
#define HFT_SIMULATOR_THREAD_AFFINITY_HPP

#include <cerrno>
#include <string>
#include <system_error>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace common {
    inline void pin_current_thread(unsigned cpu_id) {
#if defined(__linux__)
        if (cpu_id >= CPU_SETSIZE) {
            throw std::system_error(
                EINVAL,
                std::generic_category(),
                "CPU id exceeds the affinity mask capacity"
            );
        }

        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(cpu_id, &cpu_set);

        const int error = pthread_setaffinity_np(
            pthread_self(), sizeof(cpu_set), &cpu_set);
        if (error != 0) {
            throw std::system_error(
                error,
                std::generic_category(),
                "failed to pin current thread to CPU " + std::to_string(cpu_id)
            );
        }
#else
        (void) cpu_id;
        throw std::system_error(
            std::make_error_code(std::errc::operation_not_supported),
            "CPU affinity is supported only on Linux"
        );
#endif
    }
} // namespace common

#endif // HFT_SIMULATOR_THREAD_AFFINITY_HPP
