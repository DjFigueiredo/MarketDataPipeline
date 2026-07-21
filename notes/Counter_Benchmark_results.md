# System Differences #
Due to system differences it is important to understand what may happen in these tests. Some systems will be able to handle certain instructions better, may have more crossbridges, or other microarchitecture that makes it faster, etc. 
For this activity I am using a M3 Pro on MacOS. While it is unix based, it is ARM64, and as such will have some pros and cons to it. 
1. Ensure low power mode is off. If low power mode is on Mac will default to E-cores (Efficiency cores)
2. Even when trying to force MacOS to use P-cores (Performance Cores) it may schedule it somewhere else. Similarly, sometimes we require the cores to "warm up"
# Constants across Benchmarks #
- Total Increments = 10,000,000
- Number of threads = 4
- Number of runs per variant = 20
## Results ##
All results are the average of their 20 runs, with the best also being output.
# Counter Benchmark #
## Mutex Variant ##
Our mutex variant uses a basic mutex lock while incrementing the counter.
Our results are as follows:
```cpp
----------------V1 mutex start ----------------
Total time spent running: 127002625ns
BEST: ns per operation: 12.7003
AVERAGE: ns per operation: 13.2785
----------------V1 mutex end ----------------
```
## Atomic Variant ##
Out atomic variant uses a basic single atomic long int to increment.
```cpp
----------------V2 atomic start ----------------
Total time spent running: 56151417ns
BEST: ns per operation: 5.61514
AVERAGE: ns per operation: 6.0115
----------------V2 atomic end ----------------
```
## Sharded Unpadded Variant ##
Our sharded unpadded variant uses one atomic long int per thread. Now, the goal of this test is to force show the false sharing effect. As such when you declare a vector, it is possible it goes across more than one cacheline. So I used an align(64) struct to force all atomic long ints onto the same cacheline. I will explain this more in results.
```cpp
----------------V3 sharded unpadded start ----------------
Total time spent running: 52167583ns
BEST: ns per operation: 5.21676
AVERAGE: ns per operation: 5.63457
----------------V3 sharded unpadded end ----------------
```
## Sharded Padded Variant ##
Our sharded padded variant utilizes one atomic long int per thread. However, it is formed in a struct such that each thread gets its own atomic long int that is aligned to its own 64 byte address, in this case its own cacheline.
```cpp
----------------V4 sharded padded start ----------------
Total time spent running: 4876458ns
BEST: ns per operation: 0.487646
AVERAGE: ns per operation: 0.493358
----------------V4 sharded padded end ----------------
```

## Results Explained ##

**Headline:** mutex → sharded padded is a ~26x speedup (12.7ns → 0.49ns). That is the combined cost of lock contention and false sharing, eliminated one layer at a time.

**V1 Mutex (12.7ns):** Four threads serialized behind a single lock. Each increment requires acquiring the mutex, incrementing, releasing — all four threads queue for every operation. 12ns is where a contended mutex on Apple Silicon should land.

**V2 Atomic (5.6ns):** ~2.3x faster than mutex. Lock-free — no kernel primitive, just a hardware RMW on a single cache line. Still contended: all four threads hammer the same cache line, so every successful CAS invalidates that line in every other core's cache, forcing a re-fetch. This is the coherency cost of real (intentional) sharing.

**V3 Sharded Unpadded (5.2ns):** Surprisingly close to V2 rather than significantly slower. `alignas(64)` was used to deliberately force all four atomics onto the same cache line, producing false sharing — threads writing unrelated variables that happen to share a line. On x86 this would show a clear penalty on top of V2. On Apple Silicon M3 Pro, the per-cluster shared L2 resolves same-line RMW traffic more cheaply than a typical x86 cross-core bounce, so the false-sharing penalty is smaller than textbook references predict. **Cache-line addresses were printed and verified at runtime** to confirm all four atomics were colocated.

**V4 Sharded Padded (0.49ns):** ~10x faster than V3, ~26x faster than V1. Each thread's atomic gets its own `alignas(64)` cache line. No thread ever writes to a line another thread reads — zero false sharing, zero coherency traffic between threads. Each core operates entirely in its own cache without invalidating anyone else.

**Key Apple Silicon finding:** the V3→V4 gap (~10x) is real and significant, but the V2→V3 gap was near zero. On x86 with cross-core cache bouncing, V3 would typically perform worse than V2 because false sharing compounds existing contention. The per-cluster L2 shared between Apple Silicon P-cores softens that penalty. Linux/x86 CI numbers will likely show a more pronounced V2→V3 degradation.

# PingPong #
PingPong is a test where we bounce around threads using memory_order instructions. One important note is the ARM64 counter, std::memory_order_acquire (load) and std::memory_order_release (store) are pretty much the same as the actual load/store operand on x86 architecture. However, on ARM64 architecture it comes at a much higher cost. As such, we expect this test to take a bit longer for the Mac.
Results:
```cpp
p0.1: 41ns
p1: 42ns
p25: 83ns
p50: 84ns
p75: 125ns
p99: 167ns
p99.9: 375ns
```
Each sample measures one full round-trip: sender sets the flag (release store), receiver busy-polls until it sees it (acquire load), receiver responds, sender detects the response. Two persistent threads — not respawned per iteration — so thread creation cost is excluded and cache state is warm for the entire run.

**p1–p75 (42–125ns) — flat body:** The tight band across three quartiles is the steady-state handoff cost. Both threads are in a rhythm: the flag is hot in cache, the polling thread sees the update quickly, and the coherency round-trip is consistent. The ~83ns median is the hardware cost of one acquire/release cache-line bounce between two cores on Apple Silicon.

**p99/p99.9 (167ns / 375ns) — scheduling tail:** A 2x jump at p99 and 4.5x at p99.9 relative to p50. Busy-spinning threads are vulnerable to OS preemption — when the scheduler pulls one thread off-core mid-spin, the other side keeps spinning until it comes back. That forced wait shows up as a latency spike. These outliers are not code pathology; they are the OS asserting control over a spinning thread.

**p0.1 (41ns) — open question:** This floor is faster than the median by ~2x. Two candidate explanations: (1) true hardware minimum — the polling thread checked the flag at exactly the right moment, catching the release store with no extra spin iterations; (2) spin-poll variance — the receiver happened to sample the flag just as it flipped, making the "wait" effectively zero. Which explanation dominates is currently unresolved. Instruments CPU counters (cache coherency events) would help distinguish them, but that measurement has not been done yet.