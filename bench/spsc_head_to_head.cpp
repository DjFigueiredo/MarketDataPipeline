/*********************************************************************
* File:spsc_head_to_head.c
* Description: Test my SPSC queue against rigtorp / moodycamel / folly / boost
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ************************/
#include <iostream>
#include <string>
#include <cassert>
#include <thread>
#include <vector>

#include <SpscQueue.h>
#include <bench_utils.h>
#include <rigtorp/SPSCQueue.h>
#include <readerwriterqueue.h>
#include <folly/ProducerConsumerQueue.h>
#include <boost/lockfree/spsc_queue.hpp>

/**************** Constants ***********************/

constexpr long BENCH_ITERATION_COUNT_TC1  = 10000000;
constexpr int BATCH_SIZE = 64;
constexpr int NUM_BATCHES = 100000;

/**************** Structs *************************/
template <typename T, std::size_t N>
struct MySpscWrapper {
    SpscQueue<T, N> q;
    bool try_push(const T& v) { return q.try_push(v); }
    bool try_pop(T& v)        { return q.try_pop(v); }
};

template <typename T, std::size_t N>
struct RigtorpWrapper {
    rigtorp::SPSCQueue<T> q{N};          // runtime size in ctor
    bool try_push(const T& v) { return q.try_push(v); }
    bool try_pop(T& v) {
        T* p = q.front();
        if (!p) return false;
        v = *p;
        q.pop();
        return true;
    }
};

template <typename T, std::size_t N>
struct MoodyCamelWrapper {
    moodycamel::ReaderWriterQueue<T> q{N};
    bool try_push(const T& v) { return q.try_enqueue(v) ;}
    bool try_pop(T& v) { return q.try_dequeue(v); }
};

template <typename T, std::size_t N>
struct FollyWrapper {
    folly::ProducerConsumerQueue<T> q{N};
    bool try_push(const T& v) { return q.write(v) ;}
    bool try_pop(T& v) {
        T* p = q.frontPtr();
        if (!p) return false
        v = *p;
        q.popFront();
        return true;
    }
};

template <typename T, std::size_t N>
struct BoostWrapper {
    boost::lockfree::spsc_queue<T> q{N};
    bool try_push(const T& v) { return q.push(v) ;}
    bool try_pop(T& v) { return q.pop(v); }
};

/**************** Classes *************************/

/**************** Worker Functions ****************/
template <typename Queue>
void producer_thread(const long n, Queue& q) {
    pin_thread_to_core(0);
    std::size_t expected = 0;
    while (expected < n)
        if (q.try_push(static_cast<int>(expected))) {
            expected++;
        } else {
            _mm_pause();
        }
}

template <typename Queue>
void consumer_thread(const long n, Queue& q) {
    pin_thread_to_core(1);
    std::size_t expected = 0;
    int out;
    while (expected < n)
        if (q.try_pop(out)) {
            assert(out == static_cast<int>(expected));
            expected++;
        } else {
            _mm_pause();
        }
}

template <typename Queue>
void producer_req(const long n, Queue& q0, Queue& q1) {
    pin_thread_to_core(0);
    for (long i = 0; i < n; i++) {
        while (!q0.try_push(static_cast<int>(i))) {
            _mm_pause();
        }
        int echo;
        while (!q1.try_pop(echo)) {
            _mm_pause();
        }
    }
}

template <typename Queue>
void consumer_resp(const long n, Queue& q0, Queue& q1) {
    pin_thread_to_core(1);
    int val;
    for (long i = 0; i < n; i++) {
        while (!q0.try_pop(val)) {
            _mm_pause();
        }
        while (!q1.try_push(val)) {
            _mm_pause();
        }
    }
}

template <typename Queue>
void producer_burst(Queue& q) {
    pin_thread_to_core(0);
    for (int b = 0; b < NUM_BATCHES; b++) {
        for (long i = 0; i < BATCH_SIZE; i++) {
            while (!q.try_push(i)) {
                _mm_pause();
            }
        }
        sched_yield();
    }
}

template <typename Queue>
void consumer_burst(std::vector<uint64_t>& times, Queue& q) {
    pin_thread_to_core(1);
    int val;
    for (int b = 0; b < NUM_BATCHES; b++) {
        uint64_t t0 = rdtsc();
        for (long i = 0; i < BATCH_SIZE; i++) {
            while (!q.try_pop(i)) {
                _mm_pause();
            }
        }
        uint64_t t1 = rstsc();
        times.push_back(t1 - t0);
    }
}

/**************** Main Function *******************/
int main(int argc, char* argv[]) {
    // This test bench is solely for Linux at this point.
    #ifndef __linux__
        return 1;
    #endif
    if (argc != 3) {
        std::cout << "Improper Call of Binary. Example usage can be found below" << std::endl;
        std::cout << "./spsc_head_to_head <target> <test_case>" << std::endl;
        std::cout << "Allowed targets: mine | rigtorp | moodycamel | folly | boost" << std::endl;
        std::cout << "Test case 0-6:" << std::endl;
        return 1;
    }
    std::string target = argv[1];
    int test_case = std::stoi(argv[2]);

    int queue_size = 1024;
    if (test_case % 2 == 0) {
        queue_size = 512
    }

    // Use lambda as we are utilizing templates here...
    auto run = [&](auto& q0, auto& q1) {
        switch (test_case) {
            case 1, 2:
                // q1 unused
                std::thread producer([&]{ producer_thread(BENCH_ITERATION_COUNT_TC1, q0); });
                std::thread consumer([&]{ consumer_thread(BENCH_ITERATION_COUNT_TC1, q0); });
                producer.join();
                consumer.join();
                break;

            case 3,4:
                std::thread producer([&] { producer_req(BENCH_ITERATION_COUNT_TC1, q0, q1); });
                std::thread consumer([&] { consumer_resp(BENCH_ITERATION_COUNT_TC1, q0, q1); });
                producer.join();
                consumer.join();
                break;

            case 5,6:
                // q1 unused
                std::vector<uint64_t> times;
                times.reserve(NUM_BATCHES);
                std::thread producer([&]{ producer_burst(q0); });
                std::thread consumer([&]{ consumer_burst(times, q0); });
                producer.join();
                consumer.join();

                std::sort(times.begin(), times.end());
                double ghz = calibrate_tsc_ghz();
                auto to_ns = [&](uint64_t c){ return static_cast<double>(c) / ghz; };
                std::size_t times.size();
                std::cout << "P25:      " << to_ns(times[n * 25 / 100]) << " ns/n";
                std::cout << "P50:      " << to_ns(times[n * 50 / 100]) << " ns/n";
                std::cout << "P75:      " << to_ns(times[n * 75 / 100]) << " ns/n";
                std::cout << "P99:      " << to_ns(times[n * 99 / 100]) << " ns/n";
                break;
            default:
                std::cout << "Test Case provided is not valid. Please pick 1-6." << std::endl;
                return 1
        }
    };

    if (target == "mine") {
        MySpscWrapper<int, queue_size> q0;
        MySpscWrapper<int, queue_size> q1;
        run(q0, q1)
    } else if (target == "rigtorp") {
        RigtorpWrapper<int, queue_size> q0;
        RigtorpWrapper<int, queue_size> q1;
        run(q0, q1);
    } else if (target == "moodycamel") {
        MoodyCamelWrapper<int, queue_size> q0;
        MoodyCamelWrapper<int, queue_size> q1;
        run(q0, q1);
    } else if (target == "folly") {
        FollyWrapper<int, queue_size> q0;
        FollyWrapper<int, queue_size> q1;
        run(q0, q1);
    } else if (target == "boost") {
        BoostWrapper<int, queue_size> q0;
        BoostWrapper<int, queue_size> q0;
        run(q0, q1);
    } else {
        std::cout << "Target: " << target << " is not valid" << std::endl;
        return 1;
    }


    return 0;
}