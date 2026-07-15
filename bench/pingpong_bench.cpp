/* Two threads with one shared atomic flag and a paytload.
 * The goal of this benchmark is to prepare for a full SPSC queue.
 * Main idea:
 *  - Each round trip sender's turn -> write payload -> flip flah
 *      -> receiver spins until its their turn -> reads flag
 *      -> reads payload -> flips back
 *
 */

/**************** Includes ****************/
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>

/**************** Constants ****************/
constexpr int NUM_ROUND_TRIPS = 10000000;

/**************** Global Variables ****************/
std::vector<int64_t> round_trip_samples;

std::atomic_int32_t flag = {0};
int counter = 0;


/**************** Worker Functions ****************/
void sender_thread() {
    /* Initiates the round trip
     * Psuedocode:
     *  - Loop NUM_ROUND_TRIP times
     *  - capture start timestamp
     *  - flip flag to 1
     *  - spin and wait for flah to come back
     *  - capture end timestamp
     *  - push back our timing.
     */
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        const auto start = std::chrono::steady_clock::now();
        counter += 1;
        flag.store(1, std::memory_order_release);
        while(flag.load(std::memory_order_acquire) != 0);
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        round_trip_samples.push_back(static_cast<int64_t>(elapsed_ns));
    }
}

void receiver_thread() {
    /* Reacts to each signal
     * Psuedocode:
     *  - loop NUM_ROUND_TRIP times
     *  - spin until flah reads as 1
     *  - read payload
     *  - flip flag to 0
     */
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        while(flag.load(std::memory_order_acquire) != 1);
        flag.store(0, std::memory_order_release);
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
    // Loop 1million times: time one round trip, push back timing into vector.
    // Note ARM is significantly slower for acquire/release that x86 due to ISA.
    round_trip_samples.reserve(NUM_ROUND_TRIPS);
    std::thread thread1(sender_thread);
    std::thread thread2(receiver_thread);
    thread1.join();
    thread2.join();

    std::sort(round_trip_samples.begin(), round_trip_samples.end());
    std::cout << "p0.1: " << percentile(0.001) << "ns\n";
    std::cout << "p1: " << percentile(0.01) << "ns\n";
    std::cout << "p25: " << percentile(0.25) << "ns\n";
    std::cout << "p50: " << percentile(0.50) << "ns\n";
    std::cout << "p75: " << percentile(0.75) << "ns\n";
    std::cout << "p99: " << percentile(0.99) << "ns\n";
    std::cout << "p99.9: " << percentile(0.9999) << "ns\n";
    return !(counter == NUM_ROUND_TRIPS);
}