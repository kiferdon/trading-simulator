//
// Created by silay on 5/23/26.
//

#ifndef BENCHMARK_COMMON_PERF_CONTROL_HPP
#define BENCHMARK_COMMON_PERF_CONTROL_HPP
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace benchmark {
    class PerfControl {
    public:
        static void enable() {
            perf_control("enable\n");
        }

        static void disable() {
            perf_control("disable\n");
        }

    private:
        static int perf_control_fd() {
            static int fd = -2;

            if (fd != -2) {
                return fd;
            }

            const char *path = std::getenv("PERF_CTL_FIFO");
            if (path == nullptr) {
                fd = -1;
                return fd;
            }

            fd = open(path, O_WRONLY | O_CLOEXEC | O_NONBLOCK);

            if (fd == -1) {
                std::perror("open PERF_CTL_FIFO");
            }

            return fd;
        }

        static void perf_control(const char *command) {
            const int fd = perf_control_fd();
            if (fd == -1) {
                return;
            }

            const ssize_t written = write(fd, command, std::strlen(command));
            if (written == -1) {
                std::perror("perf control write");
            }
        }
    };
}

#endif //BENCHMARK_COMMON_PERF_CONTROL_HPP
