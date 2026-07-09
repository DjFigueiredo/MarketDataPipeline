
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

#if defined(__APPLE__)
#include <pthread.h>
#endif

/**************** Constants ****************/
constexpr long TOTAL_INCREMENTS = 10000000;
constexpr int NUM_THREADS = 4;
constexpr auto THREAD_INCREMENT_COUNT = TOTAL_INCREMENTS / NUM_THREADS;
constexpr int NUM_RUNS = 20;
/**************** Global Variables ****************/


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

void increment_counter_mutex(long increment_count, long long& counter, std::mutex& counter_mutex) {
    // lock_guard initializes when function comes into view of stack
    // deconstructs once function leaves stack
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    for (long n = 0; n < increment_count; n++) {
        std::lock_guard<std::mutex> guard(counter_mutex);
        counter += 1;
    }
}

long long run_mutex_variant() {
    long long counter = 0;
    std::mutex counter_mutex;
    std::vector<std::thread> thread_vect;
    for (int n = 0; n < NUM_THREADS; n++) {
        thread_vect.emplace_back(increment_counter_mutex, THREAD_INCREMENT_COUNT, std::ref(counter), std::ref(counter_mutex));
    }
    for (auto& thread_obj: thread_vect) {
        thread_obj.join();
    }
    return counter;
}

void increment_counter_atomic(long increment_count, std::atomic_int64_t& counter) {
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    for (long n = 0; n < increment_count; n++) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

long long run_atomic_variant() {
    std::atomic_int64_t counter{0};
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(counter));
    }

    for (auto& thread_obj: pool) {
        thread_obj.join();
    }
    return counter.load(std::memory_order_relaxed);
}

struct alignas(64) aligned_atomic_vector {
    // The goal of variant three is to see how there is false sharing occuring.
    // As such, if the atomic_ints were in different 64 byte cache lines, we would not actually see that.
    // This forces our vector to share a single cacheline, and as such we see the tradeoff.
    // Each core has to bounce that cacheline to the next, which is the false sharing part.
    // Hardware is also important, MAC has much better cache coherency, and as such will be faster.
    std::atomic_int64_t atomic_vector[4]{0};
};

long long run_sharded_unpadded_variant() {
    aligned_atomic_vector counter;
    std::vector<std::thread> pool;
    for (int n = 0; n < NUM_THREADS; n++) {
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(counter.atomic_vector[n]));
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
        pool.emplace_back(increment_counter_atomic, THREAD_INCREMENT_COUNT, std::ref(atomic_counters[static_cast<size_t>(n)].counter));
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



