/*********************************************************************
* File: cas_spinlock.cpp
* Description: compare and swap experiental code to teach CAS mechanic
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ****************/
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <chrono>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

/**************** Constants ****************/
constexpr int64_t NUM_ROUND_TRIPS = 10000000;
constexpr size_t NUM_THREADS = 4;

/**************** Global Variables ****************/
std::vector<std::vector<int64_t>> samples(NUM_THREADS);
std::vector<int64_t> round_trip_samples;
std::atomic<bool> flag = {false};
int64_t counter = 0;
std::vector<int64_t> acquisitions(NUM_THREADS);

/**************** Worker Functions ****************/
void cas_counter_lock() {
    bool expected = false;
    while (!flag.compare_exchange_weak(expected, true,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
#if defined(__aarch64__)
        __builtin_arm_yield();
#elif defined(__x86_64__)
        _mm_pause();
#endif
        expected = false;
    }
}

void cas_counter_unlock() {
    flag.store(false, std::memory_order_release);
}

void increment_counter(size_t thread_id) {
    for (int64_t n = 0; n < NUM_ROUND_TRIPS; n++) {
        auto start = std::chrono::steady_clock::now();
        cas_counter_lock();
        if (counter == NUM_ROUND_TRIPS) {
            cas_counter_unlock();
            break;
        }
        acquisitions[thread_id] += 1;
        counter += 1;
        cas_counter_unlock();
        auto end = std::chrono::steady_clock::now();
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        samples[thread_id].push_back(static_cast<int64_t>(elapsed_ns));
    }
}

auto percentile(double percentage) {
    if (percentage == 1) {
        return round_trip_samples[round_trip_samples.size() - 1];
    }
    double n = static_cast<double>(round_trip_samples.size());
    size_t idx = static_cast<size_t>(percentage * n);
    return round_trip_samples[idx];
}

/**************** Main Function ****************/
int main() {
    // Loop 10 million times: time one round trip, push back timing into vector.
    // Note ARM is significantly slower for acquire/release that x86 due to ISA.
    std::vector<std::thread> pool;
    round_trip_samples.reserve(static_cast<size_t>(NUM_ROUND_TRIPS));
    for (size_t n = 0; n < NUM_THREADS; n++) {
        samples[n].reserve(NUM_ROUND_TRIPS / NUM_THREADS);
    }
    for (size_t n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter, n);
    }
    for (auto& thread_obj: pool) {
        thread_obj.join();
    }
    for (auto& sample_data: samples) {
        round_trip_samples.insert(round_trip_samples.end(), sample_data.begin(), sample_data.end());
    }

    std::sort(round_trip_samples.begin(), round_trip_samples.end());
    std::cout << "p0.1: " << percentile(0.001) << "ns\n";
    std::cout << "p1: " << percentile(0.01) << "ns\n";
    std::cout << "p25: " << percentile(0.25) << "ns\n";
    std::cout << "p50: " << percentile(0.50) << "ns\n";
    std::cout << "p75: " << percentile(0.75) << "ns\n";
    std::cout << "p99: " << percentile(0.99) << "ns\n";
    std::cout << "p99.9: " << percentile(0.999) << "ns\n";
    std::cout << "| Thread ID | Acquisition Count|" << std::endl;
    std::cout << "|-----------|------------------|" << std::endl;
    for (size_t n = 0; n < NUM_THREADS; n++) {
        std::cout << "|    " << n << "      |      " <<
        (acquisitions[n]) << "     |" << std::endl;
    }

    return !(counter == NUM_ROUND_TRIPS);
}