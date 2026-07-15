## Why a memory model exists ##
The compiler and CPU reorder memory operations for speed (ECE411). Within one thread we may not notice it, but across threads reordering is more visible. **Memory model** is the rulebook for what one thread is guaranteed to see of another's writes, and `std::atomics` operations are how we guarantee that.

### The two relationships ###
- **synchronizes-with**: an acquire load that reads the value written by a release store synchronizes with that store.
- **happens-before**: if *A* synchronizes with *B*, everything before *A* in its thread is visible to everything after *B* in the other thread.
### The Memory Orders ###
#memory-orders

|   Order   | On a load                                                                               | On a store                                                             | Cost Intuition                  |
| :-------: | --------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- | ------------------------------- |
| `relaxed` | atomicity only, no ordering                                                             | atomicity only                                                         | cheapest                        |
| `acquire` | later ops in this thread can't move before it; sees the releasing thread's prior writes | n/a                                                                    | cheap on x86 (ordinary load)    |
| `release` | n/a                                                                                     | earlier ops in this thread can't move after it; publishes prior writes | cheap on x86 (ordinary store)   |
| `acq_rel` | both, for read-modify-write ops                                                         | both                                                                   | moderate                        |
| `seq_cst` | acquire+                                                                                | release+ a single global total order all threads agree on              | store needs a full fench on x86 |
**relaxed** guarantees operation is indivisible and that a single variable's modification order is consistent - nothing about other variables. 
- Use case: Counters you read after joining thread.
- Index in an SPSC queue

#### Canonical release/acquire pattern: ####
#release-acquire-pattern
```cpp
// Thread A (producer)
data = 42; //plain write
ready.store(true, std::memory_order_release); //publish

// Thread B (consumer)
if (ready.load(std::memory_order_acquire)) //observe
	use(data); //guaranteed to see 42
```

The release/acquire pair on our `ready` creates the happens-before edge that makes the plain write to `data` visible. Remove either side and thread B can legally see `ready == true` with stale `data`. 

`seq_cst` additionally promises one total order of all `seq_cst` ops that every thread agrees on. You need this only when threads must agree on the order of operations on *different* variables. Default `std::atomic` ops are `seq_cst`.

### Compare and Swap (CAS)
#compare-and-swap
```cpp
bool compare_exchange_weak(T& expected, T desired, ...);
```
Atomically: if the value equals `expected`, write `desired`, return true. Otherwise load current value into `expected`, return false.
- **weak** may fail spuriously (return false even on a match) - we can use a retry loop to fix. Cheaper on certain architectures.
- **strong** never fails spuriously - use when we are not looping.
- Typically lock-free loop: load, computer new value, CAS-weak in a `while`, repeat on failure.
**ABA problem**: you read A, another thread changes A->B->A, CAS succeeds but we missed something. Matters for lock-free stacks/lists with node reuse. Fixes: tagged/versioned pointers, hazard pointers, epochs. SPSC queue should not worry about this (monotonic indices)

### Cache lines & False sharing ###
#false-sharing #cache-lines
- Cache coherency works in 64-byte lines. Two variables on one line are one unit as far as coherency protocol goes.
- **False Sharing**: two threads with two *different* variables that share a line -> Line ping-pongs between cores in Modified state -> every write stalls. Nothing is logically shared and performance is hurt.
- Fix: `alignas(64)` on each hot variable (or padding). SPSC queue must align head_ and tail_ separately.
- Related: `std::hardware_destructive_interference_size` is the portable name for 64.
