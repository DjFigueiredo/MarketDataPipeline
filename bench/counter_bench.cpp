
/*
 * Benchmarking on macOS: disable Low Power Mode before any timed run
 * it caps max clock frequency and P-core scheduling, and produced a ~2-3x
 * slowdown in early SPSC/counter benchmarks that looked like a code regression
 * before being traced to this. pmset -g therm does not detect it
*/
/**************** Includes ****************/
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <functional>
#include <limits>
#include "bench_utils.h"

/**************** Constants ****************/
constexpr long TOTAL_INCREMENTS = 10000000;
constexpr int NUM_THREADS = 4;
constexpr auto THREAD_INCREMENT_COUNT = TOTAL_INCREMENTS / NUM_THREADS;
constexpr int NUM_RUNS = 20;

/**************** Worker Functions ****************/

void run_and_report(const char* name, std::function<long long()> variant_fn) {
    double ns_per_op_min = std::numeric_limits<double>::max();
    double ns_per_op_sum = 0;
    long long elapsed_ns_min = 0;
    for (int n = 0; n < NUM_RUNS; n++) {
        const auto start = std::chrono::steady_clock::now();
        long long value = variant_fn();
        const auto end = std::chrono::steady_clock::now();

        if (value != TOTAL_INCREMENTS) {
            std::cout << name << " Does not have the correct value: " << value << std::endl;
        }

        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double ns_per_op = static_cast<double>(elapsed_ns) / static_cast<double>(TOTAL_INCREMENTS);
        ns_per_op_sum += ns_per_op;
        if (ns_per_op < ns_per_op_min) {
            ns_per_op_min = ns_per_op;
            elapsed_ns_min = elapsed_ns;
        }
    }
    std::cout << "----------------" << name << " start ----------------" << std::endl;
    std::cout << "Total time spent running: " << elapsed_ns_min << "ns" << std::endl;
    std::cout << "BEST: ns per operation: " << ns_per_op_min << std::endl;
    std::cout << "AVERAGE: ns per operation: " << ns_per_op_sum / NUM_RUNS << std::endl;
    std::cout << "----------------" << name << " end ----------------" << std::endl;
}

void increment_counter_mutex(long increment_count, long long& counter, std::mutex& counter_mutex, int thread_idx) {
    pin_thread_to_core(thread_idx * 2 + 4);  // cores 4, 6, 8, 10 — skip HT siblings, avoid cold cores 0/2
    for (long n = 0; n < increment_count; n++) {
        std::lock_guard<std::mutex> guard(counter_mutex);
        counter += 1;
    }
}

void increment_counter_atomic(long increment_count, std::atomic_int64_t& counter, int thread_idx) {
    pin_thread_to_core(thread_idx * 2 + 4);  // cores 4, 6, 8, 10 — skip HT siblings, avoid cold cores 0/2
    for (long n = 0; n < increment_count; n++) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

long long run_mutex_variant() {
    long long counter = 0;
    std::mutex counter_mutex;
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_mutex, THREAD_INCREMENT_COUNT, std::ref(counter), std::ref(counter_mutex), n);
    }
    for (auto& t : pool) t.join();
    return counter;
}

struct alignas(64) aligned_atomic_vector {
    // Forces all four atomics onto one cache line — true 4-way false sharing.
    std::atomic_int64_t atomic_vector[4]{0};
};

long long run_atomic_variant() {
    std::atomic_int64_t counter{0};
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(counter), n);
    }
    for (auto& t : pool) t.join();
    return counter.load(std::memory_order_relaxed);
}

long long run_sharded_unpadded_variant() {
    aligned_atomic_vector counter;
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(counter.atomic_vector[n]), n);
    }
    long long counter_sum = 0;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool[static_cast<size_t>(n)].join();
        counter_sum += counter.atomic_vector[n].load(std::memory_order_relaxed);
    }
    return counter_sum;
}

struct alignas(64) single_aligned_atomic {
    std::atomic_int64_t counter{0};
};

long long run_sharded_padded_variant() {
    std::vector<single_aligned_atomic> atomic_counters(4);
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(atomic_counters[static_cast<size_t>(n)].counter), n);
    }
    long long counter_sum = 0;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool[static_cast<size_t>(n)].join();
        counter_sum += atomic_counters[static_cast<size_t>(n)].counter.load(std::memory_order_relaxed);
    }
    return counter_sum;
}

/**************** Main Function ****************/
int main() {
    run_and_report("V1 mutex",            run_mutex_variant);
    run_and_report("V2 atomic",           run_atomic_variant);
    run_and_report("V3 sharded unpadded", run_sharded_unpadded_variant);
    run_and_report("V4 sharded padded",   run_sharded_padded_variant);
    return 0;
}
