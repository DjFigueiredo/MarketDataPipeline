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

#elif defined(__APPLE__)
#include <pthread.h>

inline void pin_thread_to_core(int) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}

inline CoreSnapshot take_core_snapshot() {
    return {-1, -1};  // macOS has no per-thread core/freq query
}

#else
inline void pin_thread_to_core(int) {}
inline CoreSnapshot take_core_snapshot() { return {-1, -1}; }
#endif

inline void print_core_snapshot(const char* label, const CoreSnapshot& snap) {
    if (snap.cpu == -1) return;  // no-op on platforms that can't report
    std::cout << "  [" << label << "] core=" << snap.cpu
              << " freq=" << snap.freq_mhz << "MHz\n";
}
