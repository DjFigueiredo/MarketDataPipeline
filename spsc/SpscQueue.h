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

#ifndef CACHE_LINE_SIZE_DEFINED
#define CACHE_LINE_SIZE_DEFINED
#ifdef __APPLE__
inline constexpr std::size_t CACHE_LINE_SIZE = 128;
#else
inline constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif
#endif

/**************** Classes *************************/

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
public:
    // Producer Thread
    bool try_push(const T& item) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t - head_cache_ == Capacity) {
            // Cached guess says "Full" - copuld be stale or truth, so reload head.
            head_cache_ = head_.load(std::memory_order_acquire);
            if (t- head_cache_ == Capacity) {
                return false; // full
            }
        }
        buf_[t & (Capacity - 1)].value = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    // Consumer Thread
    bool try_pop(T& out) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_cache_) {
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (h == tail_cache_) {
                // cached guess
                return false;  // empty
            }
        }
        out = buf_[h & (Capacity - 1)].value;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // Approximate under concurrency — fine for stats, never for control flow.
    std::size_t size() const { return tail_.load(std::memory_order_relaxed) - head_.load(std::memory_order_relaxed); }

    bool empty() const { return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed); }

private:
    struct alignas(CACHE_LINE_SIZE) Slot {
        T value;
    };
    std::array<Slot, Capacity> buf_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> head_{0};
    std::size_t tail_cache_ = 0;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tail_{0};
    std::size_t head_cache_ = 0;

    /*
    After testing I realized my software was slow. The issue was loading head and tail every single time. But we do not need that - the only time we actually need that is if
    full or empty. As such we can keep a local copy of the head/tail. If it is off we never overwrite - we may skip a spot in the queue, but that is the worst case.
    As for the ordering of the declaration - easy. We want head_cache on the same line as tail - they both get wrote in the same thread.
    */
};

#endif // SPSCQUEUE_H