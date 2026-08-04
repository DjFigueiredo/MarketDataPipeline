/*********************************************************************
* File: spsc_bench.cpp
* Description: SPSC Queue Benchmark
*
* Usage: ./spsc_bench [<mode>[_printed]]
*   1[_printed]  — Item (shared cacheline) benchmark only
*   2[_printed]  — PaddedItem (own cacheline) benchmark only
*   No arg       — correctness tests + both benchmarks, printed (default)
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
#include "SpscQueue.h"
#include "bench_utils.h"

/**************** Constants ****************/
constexpr std::size_t TEST_CONCURRENCY_QUEUE_SIZE = 1024;
constexpr std::size_t TEST_CONCURRENCY_N          = TEST_CONCURRENCY_QUEUE_SIZE * 1024;

constexpr std::size_t BENCH_QUEUE_SIZE = 1024;
constexpr std::size_t BENCH_N         = 1 << 20;
constexpr int         BENCH_RUNS      = 10;

constexpr int PRODUCER_CORE = 0;
constexpr int CONSUMER_CORE = 1;

/**************** Structs *************************/
struct Item {
    int val;
};

struct alignas(CACHE_LINE_SIZE) PaddedItem {
    int val;
};

/**************** Correctness Tests ****************/

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
    std::cout << "SPSC Queue properly identifies as empty: " << queue_.empty() << "\n";
    for (std::size_t idx = 0; idx < 17; idx++) {
        bool res = queue_.try_push(static_cast<int>(idx));
        if (!res) {
            std::cout << "Expected size of the queue is: 16  Actual size is: " << queue_.size() << "\n";
            std::cout << "SPSC Queue properly returns false when trying to push when full.\n";
            return true;
        }
    }
    return false;
}

void producer_test_thread(const std::size_t n, SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE>& queue_) {
    std::size_t expected = 0;
    while (expected < n)
        if (queue_.try_push(static_cast<int>(expected))) expected++;
}

void consumer_test_thread(const size_t n, SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE>& queue_) {
    std::size_t expected = 0;
    int out;
    while (expected < n)
        if (queue_.try_pop(out)) { assert(out == static_cast<int>(expected)); expected++; }
}

void test_concurrency() {
    SpscQueue<int, TEST_CONCURRENCY_QUEUE_SIZE> queue_;
    std::thread producer(producer_test_thread, TEST_CONCURRENCY_N, std::ref(queue_));
    std::thread consumer(consumer_test_thread, TEST_CONCURRENCY_N, std::ref(queue_));
    producer.join();
    consumer.join();
    std::cout << "Concurrency test passed: " << TEST_CONCURRENCY_N << " items transferred in order.\n";
}

/**************** Benchmark ****************/

template <typename T>
void producer_bench_thread(const std::size_t n, SpscQueue<T, BENCH_QUEUE_SIZE>& queue_) {
    pin_thread_to_core(PRODUCER_CORE);
    for (std::size_t i = 0; i < n; ) {
        if (queue_.try_push(T{static_cast<int>(i)})) ++i;
        else spin_hint();
    }
}

template <typename T>
void consumer_bench_thread(const std::size_t n, SpscQueue<T, BENCH_QUEUE_SIZE>& queue_, std::vector<int64_t>& timestamps) {
    pin_thread_to_core(CONSUMER_CORE);
    T out;
    for (std::size_t i = 0; i < n; ) {
        if (queue_.try_pop(out)) {
            timestamps[i] = std::chrono::steady_clock::now().time_since_epoch().count();
            ++i;
        } else spin_hint();
    }
}

int64_t percentile(std::vector<int64_t>& samples, double p) {
    std::sort(samples.begin(), samples.end());
    if (p >= 1.0) return samples.back();
    return samples[static_cast<std::size_t>(p * static_cast<double>(samples.size()))];
}

template <typename T>
void run_benchmark(const std::string& label, bool printed) {
    int64_t best_throughput = 0;
    std::vector<int64_t> best_deltas;

    for (int r = 0; r < BENCH_RUNS; ++r) {
        SpscQueue<T, BENCH_QUEUE_SIZE> queue_;
        std::vector<int64_t> timestamps(BENCH_N);

        auto wall_start = std::chrono::steady_clock::now();
        std::thread prod(producer_bench_thread<T>, BENCH_N, std::ref(queue_));
        std::thread cons(consumer_bench_thread<T>, BENCH_N, std::ref(queue_), std::ref(timestamps));
        prod.join();
        cons.join();
        auto wall_end = std::chrono::steady_clock::now();

        int64_t elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();
        int64_t throughput = static_cast<int64_t>(
            static_cast<double>(BENCH_N) / static_cast<double>(elapsed_ns) * 1e9);

        if (throughput > best_throughput) {
            best_throughput = throughput;
            best_deltas.resize(BENCH_N - 1);
            for (std::size_t i = 1; i < BENCH_N; ++i)
                best_deltas[i - 1] = timestamps[i] - timestamps[i - 1];
        }
    }

    if (printed) {
        std::cout << "\n" << label << " (best of " << BENCH_RUNS << "):\n"
                  << "  Throughput : " << best_throughput << " ops/sec\n"
                  << "  p50        : " << percentile(best_deltas, 0.50)  << " ns\n"
                  << "  p99        : " << percentile(best_deltas, 0.99)  << " ns\n"
                  << "  p99.9      : " << percentile(best_deltas, 0.999) << " ns\n";
    }
}

/**************** Main Function *******************/
int main(int argc, char* argv[]) {
    auto [mode, printed] = parse_bench_args(argc, argv);

    if (printed) check_cpu_governor();

    // Correctness tests only on full default run
    if (mode == 0 && printed) {
        bool size_test_passed = size_test();
        std::cout << "Size Test Passed: " << size_test_passed << "\n";
        test_concurrency();
    }

    if (mode == 0 || mode == 1) run_benchmark<Item>       ("Item (shared cacheline)",      printed);
    if (mode == 0 || mode == 2) run_benchmark<PaddedItem> ("PaddedItem (own cacheline)",   printed);

    return 0;
}
