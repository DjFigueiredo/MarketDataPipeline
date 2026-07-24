/* Two threads with one shared atomic flag and a payload.
 * The goal of this benchmark is to prepare for a full SPSC queue.
 * Main idea:
 *  - Each round trip sender's turn -> write payload -> flip flag
 *      -> receiver spins until its their turn -> reads flag
 *      -> reads payload -> flips back
 */

/**************** Includes ****************/
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include "bench_utils.h"

/**************** Constants ****************/
constexpr int NUM_ROUND_TRIPS = 10000000;

/**************** Global Variables ****************/
std::vector<int64_t> round_trip_samples;

std::atomic_int32_t flag = {0};
int counter = 0;

CoreSnapshot sender_snap_start{}, sender_snap_end{};
CoreSnapshot receiver_snap_start{}, receiver_snap_end{};

/**************** Worker Functions ****************/
void sender_thread() {
    pin_thread_to_core(4);
    sender_snap_start = take_core_snapshot();
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        const auto start = std::chrono::steady_clock::now();
        counter += 1;
        flag.store(1, std::memory_order_release);
        while(flag.load(std::memory_order_acquire) != 0);
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        round_trip_samples.push_back(static_cast<int64_t>(elapsed_ns));
    }
    sender_snap_end = take_core_snapshot();
}

void receiver_thread() {
    pin_thread_to_core(6);
    receiver_snap_start = take_core_snapshot();
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        while(flag.load(std::memory_order_acquire) != 1);
        asm volatile("" : : "r,m"(counter) : "memory");
        flag.store(0, std::memory_order_release);
    }
    receiver_snap_end = take_core_snapshot();
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
    round_trip_samples.reserve(NUM_ROUND_TRIPS);
    std::thread thread1(sender_thread);
    std::thread thread2(receiver_thread);
    thread1.join();
    thread2.join();

    print_core_snapshot("sender   start", sender_snap_start);
    print_core_snapshot("sender   end  ", sender_snap_end);
    print_core_snapshot("receiver start", receiver_snap_start);
    print_core_snapshot("receiver end  ", receiver_snap_end);

    std::sort(round_trip_samples.begin(), round_trip_samples.end());
    std::cout << "p0.1: "  << percentile(0.001)  << "ns\n";
    std::cout << "p1: "    << percentile(0.01)   << "ns\n";
    std::cout << "p25: "   << percentile(0.25)   << "ns\n";
    std::cout << "p50: "   << percentile(0.50)   << "ns\n";
    std::cout << "p75: "   << percentile(0.75)   << "ns\n";
    std::cout << "p99: "   << percentile(0.99)   << "ns\n";
    std::cout << "p99.9: " << percentile(0.9999) << "ns\n";
    return !(counter == NUM_ROUND_TRIPS);
}
