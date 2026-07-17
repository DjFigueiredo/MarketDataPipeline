/*********************************************************************
* File: broken_aba.cpp
* Description: Purposefully for A->B->A break in CAS spin. Lock free,
*   First thread reads the top, sleeps. Thread 2 pops A, B, push  A back.
*   Essentially seeing how it appears we are still in order, but actually not.
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ****************/
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>
#include <chrono>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

/**************** Structs ****************/
// Node struct
struct Node {
    int value;
    Node* next;
};

// Lock-free stack
struct LockFreeStack {
    std::atomic<Node*> head = nullptr;

    void push(Node* node, int value) {
        node->value = value;
        // load current value of head into node->next
        node->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(node->next, node,
                                    std::memory_order_release,
                                    std::memory_order_relaxed)) {
#if defined(__aarch64__)
            __builtin_arm_yield();
#elif defined(__x86_64__)
            _mm_pause();
#endif
        }
    }

    Node* pop_normal() {
        Node* old = head.load(std::memory_order_acquire);
        while (old != nullptr) {
            if (head.compare_exchange_weak(old, old->next,
                                    std::memory_order_acquire,
                                    std::memory_order_relaxed)) {
                    return old;
                }
        }
        return nullptr;
    }

    // One thread will use this - sleep, then thread 2 runs.
    Node* pop_vulnerable(std::chrono::milliseconds duration) {
        Node* old = head.load(std::memory_order_acquire);
        Node* stale_next = old->next;
        std::this_thread::sleep_for(duration);
        if (head.compare_exchange_strong(old, stale_next,
                                    std::memory_order_release,
                                    std::memory_order_relaxed)) {
            std::cout << "ABA Fired: T1 committed stale next (head->B), but B was already freed." << std::endl;
            return old;
        }
        std::cout << "CAS failed — ABA did not fire this run" << std::endl;
        return nullptr;
    };
};

/**************** Global Variables ****************/
Node pool[3];
LockFreeStack stack;
/**************** Constants ****************/

/**************** Worker Functions ****************/

/**************** Main Function ****************/
int main() {
    // Build stack head -> A -> B -> C
    stack.push(&pool[2], 'C');
    stack.push(&pool[1], 'B');
    stack.push(&pool[0], 'A');

    std::thread t1([]() {
        stack.pop_vulnerable(std::chrono::milliseconds(2000));
    });

    std::thread t2([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stack.pop_normal();              // removes A, head = B -> C
        stack.pop_normal();              // removes B, head = C
        stack.push(&pool[0], 'A');       // pushes A back, A->next = C, head = A -> C
        std::cout << "T2: done. head = A -> C\n";
    });

    t1.join();
    t2.join();

    Node* final_head = stack.head.load(std::memory_order_relaxed);
    std::cout << "Final head value: " << (char)final_head->value << "\n";
    std::cout << "Expected A->C, but T1 set head to stale B\n";

    return 0;
}