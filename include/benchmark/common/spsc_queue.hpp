//
// Created by silay on 8/6/26.
//

#ifndef HFT_SIMULATOR_SPSC_QUEUE_HPP
#define HFT_SIMULATOR_SPSC_QUEUE_HPP
#include <atomic>
#include <bit>
#include <cstddef>
#include <array>

template<typename T>
concept NoThrowCopy = requires(T t)
{
    requires std::is_nothrow_copy_assignable_v<T>;
};

template<NoThrowCopy T, std::size_t Capacity>
class SpscQueue {
public:
    static_assert(Capacity >= 2);
    static_assert(std::has_single_bit(Capacity));

    SpscQueue() = default;

    SpscQueue &operator=(const SpscQueue &other) = delete;

    SpscQueue &&operator=(SpscQueue &&other) = delete;

    [[nodiscard]] bool try_push(const T &value);

    [[nodiscard]] bool try_pop(T &value);

    [[nodiscard]] bool empty() const;

private:
    std::array<T, Capacity> buffer_{};

    constexpr static std::size_t false_sharing_alignment = 128;

    alignas(false_sharing_alignment) std::atomic<std::size_t> producer_index_{0};
    alignas(false_sharing_alignment) std::atomic<std::size_t> consumer_index_{0};
};

template<NoThrowCopy T, std::size_t Capacity>
bool SpscQueue<T, Capacity>::try_push(const T &value) {
    auto loaded_pindex = producer_index_.load(std::memory_order_relaxed);
    auto loaded_cindex = consumer_index_.load(std::memory_order_acquire);

    if (loaded_pindex - loaded_cindex == Capacity) {
        return false;
    }

    auto index = loaded_pindex & (Capacity - 1);
    buffer_[index] = value;
    producer_index_.store(loaded_pindex + 1, std::memory_order_release);
    return true;
}

template<NoThrowCopy T, std::size_t Capacity>
bool SpscQueue<T, Capacity>::try_pop(T &value) {
    auto loaded_pindex = producer_index_.load(std::memory_order_acquire);
    auto loaded_cindex = consumer_index_.load(std::memory_order_relaxed);

    if (loaded_pindex == loaded_cindex) {
        return false;
    }

    auto index = loaded_cindex & (Capacity - 1);
    value = buffer_[index];
    consumer_index_.store(loaded_cindex + 1, std::memory_order_release);
    return true;
}

template<NoThrowCopy T, std::size_t Capacity>
bool SpscQueue<T, Capacity>::empty() const {
    auto loaded_pindex = producer_index_.load(std::memory_order_acquire);
    auto loaded_cindex = consumer_index_.load(std::memory_order_acquire);
    return loaded_pindex == loaded_cindex;
}
#endif //HFT_SIMULATOR_SPSC_QUEUE_HPP
