#include <atomic>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

#include "benchmark/common/spsc_queue.hpp"

namespace {
    class TestSuite {
    public:
        void expect(bool condition, std::string_view message) {
            ++checks_;
            if (!condition) {
                ++failures_;
                std::cerr << "FAIL: " << message << '\n';
            }
        }

        [[nodiscard]] int result() const {
            std::cout << "SPSC queue checks: " << checks_
                      << ", failures: " << failures_ << '\n';
            return failures_ == 0 ? 0 : 1;
        }

    private:
        int checks_ = 0;
        int failures_ = 0;
    };

    void test_empty_queue(TestSuite &suite) {
        SpscQueue<int, 4> queue;
        int value = 123;

        suite.expect(queue.empty(), "a newly constructed queue must be empty");
        suite.expect(!queue.try_pop(value), "pop from an empty queue must fail");
        suite.expect(value == 123, "failed pop must not modify the output value");
    }

    void test_capacity_and_fifo(TestSuite &suite) {
        SpscQueue<int, 4> queue;

        suite.expect(queue.try_push(10), "first push must succeed");
        suite.expect(queue.try_push(20), "second push must succeed");
        suite.expect(queue.try_push(30), "third push must succeed");
        suite.expect(queue.try_push(40), "the queue must use its full declared capacity");
        suite.expect(!queue.try_push(50), "push into a full queue must fail");
        suite.expect(!queue.empty(), "a full queue must not report empty");

        int value = 0;
        suite.expect(queue.try_pop(value) && value == 10, "FIFO mismatch for first item");
        suite.expect(queue.try_pop(value) && value == 20, "FIFO mismatch for second item");
        suite.expect(queue.try_pop(value) && value == 30, "FIFO mismatch for third item");
        suite.expect(queue.try_pop(value) && value == 40, "FIFO mismatch for fourth item");
        suite.expect(!queue.try_pop(value), "pop after draining the queue must fail");
        suite.expect(queue.empty(), "a drained queue must report empty");
    }

    void test_counter_wraparound(TestSuite &suite) {
        SpscQueue<uint64_t, 4> queue;
        uint64_t value = 0;

        for (uint64_t expected = 1; expected <= 4; ++expected) {
            suite.expect(queue.try_push(expected), "initial-cycle push failed");
        }
        for (uint64_t expected = 1; expected <= 4; ++expected) {
            suite.expect(queue.try_pop(value) && value == expected,
                         "initial-cycle FIFO mismatch");
        }

        for (uint64_t expected = 5; expected <= 8; ++expected) {
            suite.expect(queue.try_push(expected), "wrapped push failed");
        }
        for (uint64_t expected = 5; expected <= 8; ++expected) {
            suite.expect(queue.try_pop(value) && value == expected,
                         "consumer index must wrap to the corresponding array slot");
        }
    }

    void test_one_producer_one_consumer(TestSuite &suite) {
        constexpr uint64_t item_count = 1'000'000;
        SpscQueue<uint64_t, 1024> queue;
        std::atomic<bool> fifo_ok{true};
        uint64_t sum = 0;

        std::jthread consumer([&] {
            for (uint64_t expected = 1; expected <= item_count;) {
                uint64_t value = 0;
                if (!queue.try_pop(value)) {
                    std::this_thread::yield();
                    continue;
                }
                if (value != expected) {
                    fifo_ok.store(false, std::memory_order_relaxed);
                }
                sum += value;
                ++expected;
            }
        });

        for (uint64_t value = 1; value <= item_count;) {
            if (queue.try_push(value)) {
                ++value;
            } else {
                std::this_thread::yield();
            }
        }
        consumer.join();

        const uint64_t expected_sum = item_count * (item_count + 1) / 2;
        suite.expect(fifo_ok.load(std::memory_order_relaxed),
                     "concurrent transfer must preserve FIFO ordering");
        suite.expect(sum == expected_sum,
                     "concurrent transfer must not lose or duplicate items");
    }

    void test_capacity_two_structured_payload(TestSuite &suite) {
        struct Payload {
            uint64_t sequence = 0;
            uint64_t complement = 0;
        };

        constexpr uint64_t item_count = 250'000;
        SpscQueue<Payload, 2> queue;
        std::atomic<bool> payload_ok{true};

        std::jthread consumer([&] {
            for (uint64_t expected = 1; expected <= item_count;) {
                Payload payload{};
                if (!queue.try_pop(payload)) {
                    std::this_thread::yield();
                    continue;
                }

                if (payload.sequence != expected ||
                    payload.complement != ~payload.sequence) {
                    payload_ok.store(false, std::memory_order_relaxed);
                }
                ++expected;
            }
        });

        for (uint64_t sequence = 1; sequence <= item_count;) {
            if (queue.try_push(Payload{sequence, ~sequence})) {
                ++sequence;
            } else {
                std::this_thread::yield();
            }
        }
        consumer.join();

        suite.expect(payload_ok.load(std::memory_order_relaxed),
                     "capacity-two queue must preserve complete structured payloads");
        suite.expect(queue.empty(),
                     "capacity-two queue must be empty after the consumer drains it");
    }
}

int main(int argc, char **argv) {
    TestSuite suite;
    if (argc == 2 && std::string_view(argv[1]) == "wraparound") {
        test_counter_wraparound(suite);
        return suite.result();
    }

    test_empty_queue(suite);
    test_capacity_and_fifo(suite);
    test_one_producer_one_consumer(suite);
    test_capacity_two_structured_payload(suite);
    return suite.result();
}
