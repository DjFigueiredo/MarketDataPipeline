# Introduction #
#SPSC_Queue
The SPSC Queue is a `Single-Producer, Single-Consumer` lock free FIFO queue. One thread will push to the queue, and one thread will pop. It is built on a *Fixed Size ring buffer* with two atomic indices. There are no mutexs, no syscalls, no blocking, etc.

# Implementation
This document is to cover a "Paper" version of the queue before jumping into development. The following sections will cover functions and their expected functionality.

## Class Variables ##
The class we will develop is a templated Class called SpscQueue. It will maintain the following variables:
```cpp
public:
private:
std::array<T, Capacity> buf_; // ring buffer
alignas(64) std::atomic<std::size_t> head_{0}; // head idx
alignas(64) std::atomic<std::size_t> tail_{0}; // tail idx
```
Capacity is a **compile-time template parameter**, so validation must happen at compile time. We will use a `static_assert`:
```cpp
static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
```

**Free-running indices (wraparound design decision):** `head_` and `tail_` **never reset to zero**. They increment monotonically forever. Wraparound happens only when indexing into the buffer: `buf_[t & (Capacity - 1)]`. This is a deliberate choice — it makes full/empty unambiguous without a sentinel slot or a separate count:
- **Empty:** `head_ == tail_`
- **Full:** `tail_ - head_ == Capacity`

If indices reset on wrap, you need to sacrifice a slot or track a flag to distinguish full from empty. Monotonic indices avoid that entirely.

The reasoning for our alignas on tail and head is so they utilize their own cacheline, this is to avoid `false sharing`. Otherwise caches would be invalidated every other operand, and would require re-retrieving over and over.

## Public Worker Functions ##

### bool try_push(const T& item) ###
try_push is the *producer* and will be utilized to push items into the queue. That is, it we *release publishers, and acquire observers*. It shall:
1. Load `tail_` - `tail_` is only wrote by this, so we will utilize `memory_order_relaxed`.
2. Load `head_` - `head_` must be observed to determine capacity of the buffer. We are only doing a read here, so a `memory_order_acquire` is all we need.
3. If not full we will write the element into `buf_[tail & mask]` which will be a plain write indexed via bit masking.
4. Store `tail_ + 1` with `memory_order_release` to publish the element. Release guarantees the buffer write in step 3 cannot be reordered.
```cpp
bool try_push(const T& item) {
    const auto t = tail_.load(std::memory_order_relaxed);
    const auto h = head_.load(std::memory_order_acquire);
    if (t - h == Capacity) return false;  // full
    buf_[t & (Capacity - 1)] = item;
    tail_.store(t + 1, std::memory_order_release);
    return true;
}
```

### bool try_pop(T& out) ###
try_pop is our *consumer* and will be used to pop data out of our buffer. It will:
1. Load `head_`. Head only changes here, so relaxed.
2. Load `tail_` with acquire to work with the producers release. (This enforces ordering, and ensures we will see the element written before it)
3. Read the Element, store `head_ + 1` with release. 

```cpp
bool try_pop(T& out) {
    const auto h = head_.load(std::memory_order_relaxed);
    const auto t = tail_.load(std::memory_order_acquire);
    if (h == t) return false;  // empty
    out = buf_[h & (Capacity - 1)];
    head_.store(h + 1, std::memory_order_release);
    return true;
}
```


## Additional Correctness Notes ##

**Why relaxed is safe on your own index — but atomic is still required:**
No other thread *writes* your index, so there is nothing to synchronize with on the read side — relaxed suffices. But you still need `std::atomic` (not a plain variable) because the other thread *reads* it. A plain variable read from another thread is a data race; UB regardless of ordering intent.

**Why not seq_cst everywhere?**
It is correct but wasteful. `memory_order_seq_cst` forces a global total order across all seq_cst operations and on x86 emits an `mfence` or locked instruction on stores. Acquire/release creates exactly the happens-before edge SPSC needs — producer's release of `tail_` pairs with consumer's acquire — and nothing more. Paying for `mfence` here is ordering you did not ask for.

**`size()` is approximate:**
Any `size()` must read both `head_` and `tail_` as a pair. Those two reads are not atomic together — a push or pop can land in between. The result is an estimate. Never use `size()` to decide whether to push or pop; always use `try_push`/`try_pop` return values.

**Why SPSC is simpler than MPMC:**
SPSC works because each index has exactly one writer. With multiple producers, two threads could compute the same `tail_` and race to claim the same slot — a simple load-then-store is not safe. You need a CAS loop to atomically claim a slot. That CAS loop (and per-slot sequence numbers, as in Vyukov's MPMC) is the entire reason MPMC is harder. SPSC's simplicity is a direct consequence of the single-writer constraint.

# Benchmark and Testing #

For our first round of benchmarking we will measure against `std::queue`, `std::mutex`. After that we will measure against spsc queues specifically, that is:
-  [rigtorp::SPSCQueue](https://github.com/rigtorp/SPSCQueue): A clean, bounded, wait-free circular buffer queue utilizing C++11 atomics.
- [moodycamel::readerwriterqueue](https://github.com/cameron314/readerwriterqueue): A blazing-fast lock-free queue that pre-allocates blocks but can dynamically expand if needed.
- [boost::lockfree::spsc_queue](https://www.boost.org/doc/libs/1_78_0/doc/html/boost/lockfree/spsc_queue.html): Part of the trusted Boost library suite supporting compile-time or run-time sizing.
For mac we will attempt to force performance core usage; for linux we will pin the producer and consumer to different physical cores. Timing of the executions will be a per-op round trip latency with `std::chrono::steady_clock` to print out histograms. Similarly, we will attempt to do percentiles, that is the average timing of each operand to focus on the tail.