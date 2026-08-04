/* Two threads with one shared atomic flag and a payload.
 * The goal of this benchmark is to prepare for a full SPSC queue.
 * Main idea:
 *  - Each round trip sender's turn -> write payload -> flip flag
 *      -> receiver spins until its their turn -> reads flag
 *      -> reads payload -> flips back
 *
 * Usage: ./pingpong_bench [<mode>[_printed]]
 *   1          — flag and counter same cache line, silent  (perf/xctrace profiling)
 *   1_printed  — flag and counter same cache line, with output
 *   2          — flag and counter separate cache lines, silent
 *   2_printed  — flag and counter separate cache lines, with output
 *
 * Default (no args): mode 1, printed.
 */

/**************** Includes ****************/
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <string>
#include "bench_utils.h"

/**************** Constants ****************/
constexpr int NUM_ROUND_TRIPS = 10000000;

/**************** Shared State ****************/

// Mode 1: flag and counter packed — share a cache line region
struct Packed {
    std::atomic_int32_t flag{0};
    int counter{0};
};

// Mode 2: flag and counter each on their own cache line
struct Padded {
    alignas(CACHE_LINE_SIZE) std::atomic_int32_t flag{0};
    alignas(CACHE_LINE_SIZE) int counter{0};
};

static Packed g_packed;
static Padded g_padded;

std::atomic_int32_t* g_flag    = &g_packed.flag;
int*                 g_counter = &g_packed.counter;

/**************** Samples ****************/
std::vector<int64_t> round_trip_samples;

/**************** Worker Functions ****************/
void sender_thread() {
    pin_thread_to_core(0);
    auto& flag    = *g_flag;
    auto& counter = *g_counter;
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        const auto start = rdtsc();
        counter += 1;
        flag.store(1, std::memory_order_release);
        while (flag.load(std::memory_order_acquire) != 0) spin_hint();
        const auto end = rdtsc();
        round_trip_samples.push_back(static_cast<int64_t>(end - start));
    }
}

void receiver_thread() {
    pin_thread_to_core(1);
    auto& flag    = *g_flag;
    auto& counter = *g_counter;
    for (int idx = 0; idx < static_cast<int>(NUM_ROUND_TRIPS); idx++) {
        while (flag.load(std::memory_order_acquire) != 1) spin_hint();
        asm volatile("" : : "r,m"(counter) : "memory");
        flag.store(0, std::memory_order_release);
    }
}

/**************** Percentile ****************/
auto percentile(double percentage) {
    if (percentage == 1.0)
        return round_trip_samples[round_trip_samples.size() - 1];
    double n   = static_cast<double>(round_trip_samples.size());
    size_t idx = static_cast<size_t>(percentage * n);
    return round_trip_samples[idx];
}

/**************** Main Function ****************/
int main(int argc, char* argv[]) {
    int  mode    = 1;
    bool printed = true;

    if (argc > 1) {
        std::string arg(argv[1]);
        const std::string suffix = "_printed";
        if (arg.size() >= suffix.size() + 1 &&
            arg.compare(arg.size() - suffix.size(), suffix.size(), suffix) == 0) {
            printed = true;
            mode    = std::stoi(arg.substr(0, arg.size() - suffix.size()));
        } else {
            printed = false;
            mode    = std::stoi(arg);
        }
    }

    if (mode == 2) {
        g_flag    = &g_padded.flag;
        g_counter = &g_padded.counter;
    }

    double tsc_ghz = 0.0;

    if (printed) {
        check_cpu_governor();
        const char* layout = (mode == 1) ? "same cache line" : "separate cache lines";
        std::cout << "Mode " << mode << ": flag and counter on " << layout << "\n";
#ifdef __linux__
        std::cout << "Calibrating TSC...\n";
        tsc_ghz = calibrate_tsc_ghz();
        std::cout << "TSC: " << tsc_ghz << " GHz\n";
        std::cout << "Core 0: " << read_core_freq_mhz(0) << " MHz"
                  << "  Core 1: " << read_core_freq_mhz(1) << " MHz  (before)\n";
#endif
    }

    round_trip_samples.reserve(NUM_ROUND_TRIPS);
    std::thread thread1(sender_thread);
    std::thread thread2(receiver_thread);
    thread1.join();
    thread2.join();

    if (printed) {
#ifdef __linux__
        std::cout << "Core 0: " << read_core_freq_mhz(0) << " MHz"
                  << "  Core 1: " << read_core_freq_mhz(1) << " MHz  (after)\n";
#endif
        std::sort(round_trip_samples.begin(), round_trip_samples.end());
        const char* unit = timing_unit();

        auto print_pct = [&](const char* label, double pct) {
            int64_t val = percentile(pct);
            std::cout << label << val << " " << unit;
            if (tsc_ghz > 0.0)
                std::cout << " (~" << static_cast<int64_t>(
                    static_cast<double>(val) / tsc_ghz) << " ns)";
            std::cout << "\n";
        };

        print_pct("p0.1:  ", 0.001);
        print_pct("p1:    ", 0.01);
        print_pct("p25:   ", 0.25);
        print_pct("p50:   ", 0.50);
        print_pct("p75:   ", 0.75);
        print_pct("p99:   ", 0.99);
        print_pct("p99.9: ", 0.999);
    }

    return !(*g_counter == NUM_ROUND_TRIPS);
}
