# ARM64 vs x86 Synchronization: What the Numbers Actually Say

---

## Introduction

Recently I rotated through an engineering team at Caterpillar Inc. where I developed low-level software for Telematics Engine Control Units on x86 processors running Linux. That was a step outside my normal path, I had focused on low-level software for RTOS machines.

Moving into lower-latency systems work raised a question I couldn't answer from documentation alone: how much does ISA-level memory ordering actually cost, and does the ARM64/x86 gap matter in practice?

The ARM Architecture Reference Manual specifies that `memory_order_acquire` and `memory_order_release` map to `ldapr`/`stlr` — explicit barrier instructions with real pipeline cost. Intel's SDM Vol. 3A §8 describes x86's TSO model: the hardware enforces most ordering constraints, so acquire/release map to plain loads and stores. Different contracts. I wanted the measured delta, not the architectural description.

I ran three benchmarks on two physical machines: a MacBook Pro M3 Pro (ARM64, macOS) and a Dell Precision 3640 running Ubuntu bare metal (Intel 10th Gen Comet Lake, x86-64). Bare metal matters, a hypervisor remaps vCPUs between physical cores and pollutes both pinning assumptions and tail latency.

What follows are the results from a counter contention ladder (V1–V4), a pingpong acquire/release round-trip, and an SPSC queue throughput test, with the mechanism behind each number.

---

## Hardware Specifications

|                | MacBook Pro (ARM64)                                                               | Dell Precision 3640 (x86-64)                                 |
| -------------- | --------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| CPU            | Apple M3 Pro (6P + 6E cores, 12 total)                                            | Intel Core i9-10900 @ 2.80GHz (10C/20T, 10th Gen Comet Lake) |
| Architecture   | ARM64                                                                             | x86-64                                                       |
| OS             | macOS Sequoia 15.7.3                                                              | Ubuntu 24.04 (bare metal)                                    |
| RAM            | 36 GB                                                                             | 32 GB                                                        |
| Kernel         | 24.6.0                                                                            | 7.0.0-28-generic                                             |
| Cache Line     | 128 bytes (measured via `sysctl`)                                                 | 64 bytes                                                     |
| L1d / L1i      | P-core: 128 KiB / 192 KiB; E-core: 64 KiB / 128 KiB (measured; no published spec) | 32 KiB / 32 KiB per core                                     |
| L2             | P-core: 16 MiB per cluster; E-core: 4 MiB (measured; no published spec)           | 256 KiB per core                                             |
| L3             | None (inferred; no published spec)                                                | 20 MiB (shared)                                              |
| Timer          | `mach_absolute_time` — 24 MHz, ~42 ns/tick                                        | `rdtscp` — serialized cycle counter                          |
| Thread Pinning | `pthread_set_qos_class_self_np` (QoS hint only — no hard affinity API)            | `pthread_setaffinity_np`                                     |
| Core Isolation | N/A                                                                               | `isolcpus=0,1,2,3` + `nohz_full` + `rcu_nocbs`               |

**Target word count: ~150 words**

**Guideline:** Two-column prose or a compact table: Mac (M3 Pro, ARM64, macOS, QoS-hint-only pinning, 24MHz `mach_absolute_time`) vs Linux x86 (Intel 10th Gen Comet Lake, Dell Precision 3640, Ubuntu bare metal, `pthread_setaffinity_np`, `isolcpus=0,1,2,3` + `nohz_full` + `rcu_nocbs`, `rdtscp` cycle counter).

Call out the key methodological asymmetry: macOS has no hard thread-affinity API; Linux has isolated cores. This asymmetry is load-bearing for interpreting tail latency differences — say so here so the results section doesn't have to re-explain it.

Cite this section's source as DJ's own platform configuration, not a third-party doc.

**Apple Silicon flag:** Any claim about M3 Pro microarchitecture internals (P-core cluster, shared L2) must be tagged as "measured/inferred — not from published specs."

---

## Testing

**Target word count: ~850 words across all subsections**

**Guideline:** Three benchmarks in order — counter_bench, pingpong_bench, spsc_bench. Each subsection: (1) what it measures and why, (2) results table, (3) mechanism explanation tied directly to numbers, (4) cross-platform comparison. Lean on tables for data; prose carries only the mechanism and the "why this number." Every sentence should be carrying a number, a mechanism, or a citation.

---

### Counter Benchmark (V1–V4)

**Target word count: ~300 words**

**Guideline:** State what each variant isolates — V1 serializes behind one mutex, V2 uses a single shared atomic (lock-free RMW but all threads hammer one cache line), V3 shards the counter but leaves shards on the same cache line (false sharing), V4 pads each shard to its own cache line. The progression is deliberate: each variant removes one bottleneck so the reader can see exactly what each layer costs.

Results table (fill in from `test_results_mac.txt` and `test_results_linux.txt`):

| Variant                | Mac Best (ns/op) | Mac Avg (ns/op) | Linux Best (ns/op) | Linux Avg (ns/op) |
| ---------------------- | ---------------- | --------------- | ------------------ | ----------------- |
| V1 — mutex            |                  |                 |                    |                   |
| V2 — atomic (shared)  |                  |                 |                    |                   |
| V3 — sharded unpadded |                  |                 |                    |                   |
| V4 — sharded padded   |                  |                 |                    |                   |

Key findings to cover in prose:

- V1→V4 headline speedup on each platform (state the multiplier, both platforms)
- V2→V3 gap: near-zero on Apple Silicon, more pronounced on Linux — per-cluster shared L2 hypothesis explains the Mac result (flag as "inferred"); x86 private-L2 + shared-L3-via-uncore is the contrasting mechanism
- V3→V4 gap: the primary false-sharing signal — consistent on both platforms because padding is an ISA-independent physical effect
- Variance as signal: V1/V4 show ~1–4% run-to-run spread; V2/V3 show ~10–11% spread despite similar means — contention-heavy variants are noisier, not just slower

**Apple Silicon flag:** The per-cluster shared L2 claim must be tagged "inferred — not from published specs." Reference Dougall Johnson's reverse-engineering work as the best available public source for Apple Silicon microarchitecture.

Cite: ARM Architecture Reference Manual (barrier semantics) and Intel SDM Vol. 3A §8 (memory ordering) as the ISA contracts that ground the mechanism explanation.

---

### PingPong Benchmark (acquire/release round-trip cost)

**Target word count: ~280 words**

**Guideline:** State what it measures: one `memory_order_acquire` / `memory_order_release` cache-line handoff round trip between two pinned threads. The p50 is the hardware cost of the primitive; the tail is OS scheduling noise. Separate these two clearly — they are different phenomena.

Results table (fill in from test results; Linux values in cycles with ~ns conversion at 3.7GHz):

| Percentile | Mac (ns) | Linux (cycles) | Linux (~ns @ 3.7GHz) |
| ---------- | -------- | -------------- | -------------------- |
| p0.1       |          |                |                      |
| p1         |          |                |                      |
| p25        |          |                |                      |
| p50        |          |                |                      |
| p75        |          |                |                      |
| p99        |          |                |                      |
| p99.9      |          |                |                      |

Key findings to cover in prose:

- ISA cost at p50: ARM64 requires explicit `ldapr`/`stlr` barrier instructions; x86 TSO gives acquire/release semantics near-free — plain loads/stores, hardware enforces ordering. State the ISA contract difference; cite ARM Architecture Reference Manual and Intel SDM Vol. 3A §8. Do not derive from first principles.
- Tail (p99.9) is OS scheduling noise, not a property of the synchronization primitive. Quantify: before `isolcpus` on Linux, p99.9 reached ~40K cycles; after isolated cores, collapsed to ~3K cycles — 13x reduction from kernel configuration, not code change.
- Mac p99.9 stability: `spin_hint()` contribution — 7 of 8 runs bit-identical at same value.

**Apple Silicon flag:** The p50 Mac number is a direct measurement. Any inference about why (P-core coherency cost) is inferred — tag it.

---

### SPSC Queue Benchmark

**Target word count: ~270 words**

**Guideline:** State what it measures: throughput (ops/sec) and inter-arrival delta percentiles for a 1024-slot lock-free SPSC ring buffer, comparing `Item` (shares a cache line with adjacent slots) vs `PaddedItem` (`alignas(64)`, own 64-byte cache line). Producer pinned to core 0, consumer to core 1. Best-of-10 harness.

Results table (fill in from test results):

|                      | Mac — Item | Mac — PaddedItem | Linux — Item | Linux — PaddedItem |
| -------------------- | ----------- | ----------------- | ------------- | ------------------- |
| Throughput (ops/sec) |             |                   |               |                     |
| p50 (ns)             |             |                   |               |                     |
| p99 (ns)             |             |                   |               |                     |
| p99.9 (ns)           |             |                   |               |                     |

Key findings to cover in prose:

- Item wins on throughput on both platforms — state the delta (%)
- Why: `PaddedItem` buffer at 1024 slots × 64 bytes = 64KB overflows L1 cache (~32KB on x86, similar on Apple Silicon — **inferred for Apple Silicon**). Cache capacity cost outweighs false-sharing penalty at this queue size.
- PaddedItem shows tighter tail on Linux (p99.9): false-sharing penalty appears in latency variance, not throughput, once L1 overflow dominates throughput cost.
- Mac latency values are quantized to ~42ns tick multiples (24MHz timer floor). Throughput is reliable; latency percentiles are tick counts, not exact nanoseconds.

Cite: DJ's benchmark source (`spsc_bench.cpp`) as the measurement source for all numbers in this section.

---

## What the Hardware Docs Say

**Target word count: ~150 words**

**Guideline:** One paragraph per source, three sources total. Keep to one paragraph each — not a textbook chapter.

- **Intel:** SDM Vol. 3A §8 (memory ordering rules, StoreLoad as the one reordering x86 permits) + Optimization Reference Manual (pause instruction, spin-loop guidance). Both fully public, cite directly.
- **ARM:** Architecture Reference Manual, barrier instruction section — `DMB`/`DSB`/`ISB` semantics, `ldapr`/`stlr` for acquire/release. Cite directly.
- **Apple:** No published microarchitecture manual exists — state this explicitly. Reference Dougall Johnson's reverse-engineering work as the best available public source, framed as community reverse-engineering, not official specs.

This is the one section where citing the source documents at depth is appropriate. Every mechanism claim in the Testing section should be traceable to one of these three sources or to DJ's own measurements.

---

## Takeaways for Systems Engineers

**Target word count: ~100 words**

**Guideline:** Three declarative bullets, each tied to a specific numbered finding from the Testing section above. No adjectives doing work that numbers should do. Candidate content:

- Benchmark on your actual target hardware — results don't transfer across ISAs (tie to specific counter_bench or pingpong delta)
- Variance between runs is a signal to investigate, not noise to suppress — contention-heavy variants are noisier, not just slower (tie to V2/V3 spread finding)
- Kernel-level isolation (`isolcpus` + `nohz_full`) compresses tail latency more than any code change at this scale — tie to the 13x p99.9 reduction from configuration alone

Keep it short. If a sentence can't be tied to a number, a mechanism, or a citation from the sections above, cut it.
