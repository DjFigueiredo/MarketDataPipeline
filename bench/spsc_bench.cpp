/*********************************************************************
* File: spsc_bench.cpp
* Description: SPSC Queue Benchmark testing
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ************************/
#include <array>
#include <random>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <algorithm>
#include <string>
#ifdef __linux__
#include <pthread.h>
#endif
#include "SpscQueue.h"

/**************** Constants ****************/
constexpr std::size_t TEST_CONCURRENCY_QUEUE_SIZE = 1024;
constexpr std::size_t TEST_CONCURRENCY_N          = TEST_CONCURRENCY_QUEUE_SIZE * 1024; // 1M items

constexpr std::size_t BENCH_QUEUE_SIZE = 1024;
constexpr std::size_t BENCH_N         = 1 << 20; // 1M items

constexpr int PRODUCER_CORE = 0;
constexpr int CONSUMER_CORE = 1;

/**************** Structs *************************/
struct Item {
    int val;
}; // Shared cacheline

struct alignas(CACHE_LINE_SIZE) PaddedItem {
    int val;
}; // own cacheline

/**************** Worker Functions ****************/

template <typename T, std::size_t N>
std::array<T, N> randomized_array() {
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 1000000);
    std::array<T, N> arr;
    for (auto& slot : arr) slot = T{dist(rng)};
    return arr;
}

bool size_test() {
    SpscQueue<int, 16> queue_;
    std::cout << "SPSC Queue properly identifies as empty: " << queue_.empty() << std::endl;
    for (std::size_t idx = 0; idx < 17; idx++) {
        bool res = queue_.try_push(static_cast<int>(idx));
        if (!res) {
            std::cout << "Expected size of the queue is: " << 16 << " Actual size is: " << queue_.size() << std::endl;
            std::cout << "SPSC Queue properly returns false when trying to push when full." << std::endl;
            return true;
        }
    }
    return false;
}

void producer_test_thread(const std::size_t n, SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE>& queue_) {
    std::size_t expected = 0;
    while (expected < n) {
        if (queue_.try_push(static_cast<int>(expected))) {
            expected++;
        }
    }
}

void consumer_test_thread(const size_t n, SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE>& queue_) {
    std::size_t expected = 0;
    int out;
    while (expected < n) {
        if (queue_.try_pop(out)) {
            assert(out == static_cast<int>(expected));
            expected++;
        }
    }
}

void test_concurrency() {
    SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE> queue_;
    std::thread producer(producer_test_thread, TEST_CONCURRENCY_N, std::ref(queue_));
    std::thread consumer(consumer_test_thread, TEST_CONCURRENCY_N, std::ref(queue_));
    producer.join();
    consumer.join();
    std::cout << "Concurrency test passed: " << TEST_CONCURRENCY_N << " items transferred in order." << std::endl;
}

void pin_thread(int core_id) {
#ifdef __APPLE__
    // macOS has no hard affinity API — no-op
    (void)core_id;
    pthread_set_qos_class_self_np(QoS_CLASS_USER_INTERACTIVE, 0);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

template <typename T>
void producer_bench_thread(const std::size_t n, SpscQueue<T, BENCH_QUEUE_SIZE>& queue_) {
    pin_thread(PRODUCER_CORE);
    for (std::size_t i = 0; i < n; ) {
        if (queue_.try_push(T{static_cast<int>(i)})) ++i;
    }
}

template <typename T>
void consumer_bench_thread(const std::size_t n, SpscQueue<T, BENCH_QUEUE_SIZE>& queue_, std::vector<int64_t>& timestamps) {
    pin_thread(CONSUMER_CORE);
    T out;
    for (std::size_t i = 0; i < n; ) {
        if (queue_.try_pop(out)) {
            timestamps[i] = std::chrono::steady_clock::now().time_since_epoch().count();
            ++i;
        }
    }
}

int64_t percentile(std::vector<int64_t>& samples, double p) {
    std::sort(samples.begin(), samples.end());
    if (p >= 1.0) return samples.back();
    return samples[static_cast<std::size_t>(p * static_cast<double>(samples.size()))];
}

template <typename T>
void run_benchmark(const std::string& label) {
    SpscQueue<T, BENCH_QUEUE_SIZE> queue_;
    std::vector<int64_t> timestamps(BENCH_N);

    auto wall_start = std::chrono::steady_clock::now();
    std::thread prod(producer_bench_thread<T>, BENCH_N, std::ref(queue_));
    std::thread cons(consumer_bench_thread<T>, BENCH_N, std::ref(queue_), std::ref(timestamps));
    prod.join();
    cons.join();
    auto wall_end = std::chrono::steady_clock::now();

    int64_t elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();
    double throughput  = static_cast<double>(BENCH_N) / static_cast<double>(elapsed_ns) * 1e9;

    // Inter-arrival deltas between consecutive consumer pops as latency proxy
    std::vector<int64_t> deltas(BENCH_N - 1);
    for (std::size_t i = 1; i < BENCH_N; ++i) {
        deltas[i - 1] = timestamps[i] - timestamps[i - 1];
    }

    std::cout << "\n" << label << ":\n"
              << "  Throughput : " << static_cast<int64_t>(throughput) << " ops/sec\n"
              << "  p50        : " << percentile(deltas, 0.50)  << " ns\n"
              << "  p99        : " << percentile(deltas, 0.99)  << " ns\n"
              << "  p99.9      : " << percentile(deltas, 0.999) << " ns\n";
}

void shared_cacheline_benchmark() {
    run_benchmark<Item>("Item (shared cacheline)");
}

void padded_benchmark() {
    run_benchmark<PaddedItem>("PaddedItem (own cacheline)");
}

/**************** Main Function *******************/
int main() {
    bool size_test_passed = size_test();
    std::cout << "Size Test Passed: " << size_test_passed << std::endl;
    test_concurrency();
    shared_cacheline_benchmark();
    padded_benchmark();
    return 0;
}
