/*********************************************************************
* File: SpscQueue.h
* Description: Single Producer, Single Consumer templated implementation.
* Prints:
* Returns:
*********************************************************************/

#ifndef SPSCQUEUE_H
#define SPSCQUEUE_H
/**************** Includes ************************/
#include <array>
#include <atomic>
#include <cstddef>
#include <new>

/**************** Classes *************************/

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
public:
    // Producer Thread
    bool try_push(const T& item) {
        const auto t = tail_.load(std::memory_order_relaxed);
        const auto h = head_.load(std::memory_order_acquire);
        if (t - h == Capacity) {
            return false;  // full
        }
        buf_[t & (Capacity - 1)] = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    // Consumer Thread
    bool try_pop(T& out) {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (h == t) {
            return false;  // empty
        }
        out = buf_[h & (Capacity - 1)];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // Approximate under concurrency — fine for stats, never for control flow.
    std::size_t size() const { return tail_.load(std::memory_order_relaxed) - head_.load(std::memory_order_relaxed); }

    bool empty() const { return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed); }

private:
    // We could have used a struct here to force each "item" to be on its own cacheline,
    // However, we will instead use the same test cases twice, one that is aligned, and the
    // Other not aligned to see the true tradeoff.
    std::array<T, Capacity> buf_;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head_{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail_{0};
};

#endif // SPSCQUEUE_H