/*********************************************************************
* File:bench_utils.h
* Description: Shared benchmark utilities — thread pinning and core stats
*********************************************************************/
#pragma once

#include <iostream>
#include <string>

/**************** Structs *************************/

struct CoreSnapshot {
    int cpu     = -1;
    long freq_mhz = -1;
};

/**************** Worker Functions ****************/

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <fstream>
#include <immintrin.h>

inline void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
        std::cerr << "pin_thread_to_core: failed to pin to core " << core_id << "\n";
}

inline CoreSnapshot take_core_snapshot() {
    int cpu = sched_getcpu();
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/scaling_cur_freq";
    std::ifstream f(path);
    long freq_khz = -1;
    f >> freq_khz;
    return {cpu, freq_khz / 1000};
}

inline void spin_hint() {
    _mm_pause();
}

inline uint64_t rdtsc() {
    unsigned int lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

inline const char* timing_unit() { return "cycles"; }

inline void check_cpu_governor() {
    std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    std::string gov;
    if (f >> gov && gov != "performance")
        std::cerr << "WARNING: CPU governor is '" << gov
                  << "' — run: sudo cpupower frequency-set -g performance\n";
}

#elif defined(__APPLE__)
#include <pthread.h>
#include <mach/mach_time.h>

inline void pin_thread_to_core(int) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}

inline CoreSnapshot take_core_snapshot() {
    return {-1, -1};  // macOS has no per-thread core/freq query
}

inline void spin_hint() {
    __builtin_arm_yield();
}

// mach_absolute_time() ticks at 24MHz on Apple Silicon (~42ns resolution).
// Timebase converts ticks to nanoseconds: result = ticks * numer / denom.
inline uint64_t rdtsc() {
    static mach_timebase_info_data_t tb = [] {
        mach_timebase_info_data_t t;
        mach_timebase_info(&t);
        return t;
    }();
    return mach_absolute_time() * tb.numer / tb.denom;
}

inline const char* timing_unit() { return "ns (24MHz, ~42ns floor)"; }
inline void check_cpu_governor() {}

#else
#include <chrono>
inline void pin_thread_to_core(int) {}
inline CoreSnapshot take_core_snapshot() { return {-1, -1}; }
inline void spin_hint() {}
inline uint64_t rdtsc() {
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}
inline const char* timing_unit() { return "ns"; }
inline void check_cpu_governor() {}
#endif

inline void print_core_snapshot(const char* label, const CoreSnapshot& snap) {
    if (snap.cpu == -1) return;  // no-op on platforms that can't report
    std::cout << "  [" << label << "] core=" << snap.cpu
              << " freq=" << snap.freq_mhz << "MHz\n";
}
