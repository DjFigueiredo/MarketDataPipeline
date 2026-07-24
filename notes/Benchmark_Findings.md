#benchmarking #memory-model #false-sharing #cache-lines #macos

# Threading Benchmark Findings — Apple Silicon M3 Pro

Hardware: MacBook Pro M3 Pro, macOS, ARM64
Methodology: N=20 in-process runs per variant, best + average reported

## Hardware Notes

Apple Silicon M3 Pro has characteristics that produce results that diverge from x86-focused textbook descriptions:
- P-cores and E-cores share the same physical package. **Low Power Mode forces scheduling onto E-cores**, producing 2-3x slowdowns that look exactly like code regressions. `pmset -g therm` does not detect this — it queries Intel-era SMC sensors that don't exist on Apple Silicon. Check System Settings > Battery directly before trusting any benchmark number.
- P-cores within the same cluster share an L2 cache. This softens the false-sharing penalty relative to x86 cross-core cache bouncing.
- macOS has no hard thread-affinity API. `pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE)` was tested as a scheduling hint — succeeded on every call but did not measurably tighten run-to-run variance on contended variants. Consistent with it being a hint, not a guarantee.
- **In-process sampling beats cross-process sampling.** Taking best-of-N within one binary invocation is more stable than relaunching N times. V1's floor dropped from ~20-28ns (cross-process) to ~13ns (in-process) with identical code — less exposure to inter-run OS/thermal entropy.

---

# Counter Benchmark

10,000,000 total increments, 4 threads, 20 runs per variant.

| Variant | Best (ns/op) | Average (ns/op) | Spread |
|---|---|---|---|
| V1 — mutex | 12.70 | 13.28 | ~4% |
| V2 — single atomic | 5.62 | 6.01 | ~10% |
| V3 — sharded unpadded (forced colocated) | 5.22 | 5.63 | ~11% |
| V4 — sharded padded (`alignas(64)`) | 0.49 | 0.49 | ~1% |

**Headline:** mutex → sharded padded is a ~26x speedup (12.7ns → 0.49ns). That is the combined cost of lock contention and false sharing, eliminated one layer at a time.

## Variant Analysis

**V1 Mutex (12.7ns):** Four threads serialized behind a single lock. Each increment requires acquiring the mutex, incrementing, releasing — all four threads queue for every operation. 12ns is where a contended mutex on Apple Silicon lands.

**V2 Atomic (5.6ns):** ~2.3x faster than mutex. Lock-free — no kernel primitive, just a hardware RMW on a single cache line. Still contended: all four threads hammer the same cache line, so every successful CAS invalidates that line in every other core's cache, forcing a re-fetch. This is the coherency cost of real (intentional) sharing.

**V3 Sharded Unpadded (5.2ns):** Surprisingly close to V2. The goal was to demonstrate false sharing — threads writing unrelated variables that happen to share a cache line. Two attempts were required:

- *First attempt* (plain `atomic_int64_t[4]`, no forcing): addresses landed as two pairs across two separate cache lines — partial 2-way false sharing only. Result: **~1.9ns/op**, much faster than expected, because only half the threads were actually contending on each line.
- *Forced colocation* (whole array wrapped in one `alignas(64)` struct): all four addresses landed on a single 64-byte line, confirmed by printing addresses at runtime (`...c80, c88, c90, c98`). True 4-way false sharing. Result: **5.22ns/op**.

Cache-line addresses were printed and verified — not assumed.

On x86 this would show a more severe penalty than V2. On Apple Silicon M3 Pro, the per-cluster shared L2 resolves same-line RMW traffic more cheaply than a typical x86 cross-core bounce, so the false-sharing cost lands close to V2 rather than close to V1.

**V4 Sharded Padded (0.49ns):** `alignas(64)` on each individual element — addresses landed exactly 64 bytes apart (`...240, 280, 2c0, 300`), confirming per-element cache-line isolation. No thread ever writes to a line another thread reads. Zero false sharing, zero coherency traffic between threads. Each core operates entirely in its own cache.

## Variance as a Signal

V1 and V4 — the two settled mechanisms (full serialization vs. full isolation) — show 1-4% run-to-run spread. V2 and V3 — both under real cache-line contention — show ~10-11% spread despite similar means. Contention-heavy variants are noisier, not just slower. Worth checking spread, not just mean, when characterizing a synchronization primitive.

## Key Apple Silicon Finding

The V3→V4 gap (~10x) is real and significant. The V2→V3 gap was near zero. On x86 with cross-core cache bouncing, V3 would typically land worse than V2 because false sharing compounds existing contention. The per-cluster L2 shared between Apple Silicon P-cores softens that penalty. Linux/x86 CI numbers will likely show a more pronounced V2→V3 degradation — to be confirmed once CI is running.

---

# PingPong Benchmark

Two persistent threads (not respawned per iteration), one `atomic_int32_t` flag, acquire/release synchronization. Each sample = one full round-trip: sender stores flag (release), receiver busy-polls until it sees it (acquire), receiver responds, sender detects response. 10,000,000 round trips.

```
p0.1:  41ns
p1:    42ns
p25:   83ns
p50:   84ns
p75:  125ns
p99:  167ns
p99.9: 375ns
```

One important note: `memory_order_acquire` (load) and `memory_order_release` (store) map directly to load/store instructions on x86 — effectively free. On ARM64 they require explicit barrier instructions and come at a real cost. These numbers reflect that ARM64 overhead.

**p1–p75 (42–125ns) — flat body:** Tight band across three quartiles. Both threads are in a steady rhythm: the flag is hot in cache, the polling thread sees the update quickly, coherency round-trip is consistent. The ~83ns median is the hardware cost of one acquire/release cache-line bounce between two P-cores on Apple Silicon.

**p99/p99.9 (167ns / 375ns) — scheduling tail:** 2x jump at p99, 4.5x at p99.9 relative to p50. Busy-spinning threads are vulnerable to OS preemption — when the scheduler pulls one thread off-core mid-spin, the other side spins until it comes back. These outliers are not code pathology; they are the OS asserting control over a spinning thread.

**p0.1 (41ns) — open question:** Floor is ~2x faster than median. Two candidate explanations: (1) true hardware minimum — polling thread caught the release store with no extra spin iterations; (2) spin-poll variance — receiver sampled the flag just as it flipped, making wait effectively zero. Currently unresolved. Instruments CPU Counters (cache coherency events) would help distinguish, but that measurement has not been done yet.

---

# Interview Summary

- "I benchmarked mutex vs. atomic vs. sharded counters on real hardware and measured false sharing directly — from cache-line addresses I printed myself, not from a textbook."
- "The false-sharing penalty was smaller than expected on Apple Silicon vs. x86-focused writeups — I have a hardware hypothesis (shared per-cluster L2) but haven't confirmed it on x86 yet."
- "I learned the hard way that Low Power Mode produces a 2-3x swing that looks exactly like a code regression, and that `pmset -g therm` doesn't catch it on Apple Silicon."
- "Contention shows up in variance, not just mean latency — the two contended variants had 10%+ spread, the two settled ones had under 4%."
- "The pingpong p50 of 84ns is the raw cost of one acquire/release cache-line handoff between two ARM64 cores. The p99.9 tail at 375ns is OS scheduling, not the synchronization primitive."
