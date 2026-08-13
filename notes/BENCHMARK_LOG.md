# Benchmark Data Log

All raw output and derived numbers in one place.
Organized by platform → tool → benchmark. Computed values are clearly labeled.

---

## Platform Notes

### Linux (Intel i9-10900, x86-64, Ubuntu 24.04 bare metal)

- **TSC:** 2.808 GHz (calibrated by `calibrate_tsc_ghz()` via `CLOCK_MONOTONIC_RAW`)
- **`scaling_cur_freq` during these runs:** showed 800 MHz — this is stale/incorrect
- **Actual CPU frequency (inferred from perf stat cycles ÷ wall time):**
  - Counter benchmark (4 cores): ~4.47 GHz
  - Pingpong benchmark (2 cores): ~4.91 GHz
  - SPSC benchmark (2 cores): ~4.80 GHz
  - Consistent with i9-10900 multi-core turbo (4-core ~4.5 GHz, 2-core ~4.8–5.0 GHz)
- **`scaling_cur_freq` is unreliable on this machine with `intel_pstate` in active mode.**
  It reflects an idle P-state snapshot, not the actual boost frequency during the workload.
  Use (perf stat cycles ÷ wall time) as the authoritative frequency check.
- **Core isolation:** `isolcpus=0,1,2,3 nohz_full=0,1,2,3 rcu_nocbs=0,1,2,3`
- **Turbo:** enabled (`no_turbo=0`)
- **Governor:** performance (`sudo cpupower frequency-set -g performance`)

### Mac (Apple M3 Pro, ARM64, macOS Sequoia 15.7.3)

- **Timer:** `mach_absolute_time()` via `mach_timebase_info` — 24 MHz, ~42 ns/tick
- **Quantization floor:** all latency values are multiples of ~41.7 ns (one tick)
- **Thread pinning:** `pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0)` — hint only
- **Low Power Mode:** disabled for all runs
- **Cache line:** 128 bytes (`hw.cachelinesize` via `sysctl`)
- **`CACHE_LINE_SIZE` constant:** 128 (vs 64 on Linux)

---

## Linux — `perf stat` Raw Output

**Command used for all:**
```
perf stat -e cycles,instructions,LLC-load-misses,LLC-loads ./<bench> <mode>
```

Run date: 2026-08-04

### Pingpong Benchmark

```
Performance counter stats for './pingpong_bench 1':

    13,598,193,876      cycles
       704,558,492      instructions
            20,135      LLC-load-misses
        20,093,729      LLC-loads

       1.385193281 seconds time elapsed
       2.742883000 seconds user
       0.023130000 seconds sys


Performance counter stats for './pingpong_bench 2':

    13,344,912,027      cycles
       692,502,880      instructions
            16,752      LLC-load-misses
        20,293,034      LLC-loads

       1.343187739 seconds time elapsed
       2.664375000 seconds user
       0.018125000 seconds sys
```

### Counter Benchmark

```
Performance counter stats for './counter_bench 1':

   103,027,010,844      cycles
    67,933,474,958      instructions
             7,684      LLC-load-misses
       219,050,476      LLC-loads

       7.512383383 seconds time elapsed
       9.270233000 seconds user
      15.702939000 seconds sys


Performance counter stats for './counter_bench 2':

    65,706,589,314      cycles
       621,150,701      instructions
             6,264      LLC-load-misses
           262,796      LLC-loads

       3.622228751 seconds time elapsed
      13.672212000 seconds user
       0.006004000 seconds sys


Performance counter stats for './counter_bench 3':

    67,600,825,816      cycles
       623,506,785      instructions
             5,449      LLC-load-misses
           253,610      LLC-loads

       3.678937874 seconds time elapsed
      14.072527000 seconds user
       0.000000000 seconds sys


Performance counter stats for './counter_bench 4':

     3,634,514,575      cycles
       518,258,153      instructions
             4,218      LLC-load-misses
           120,372      LLC-loads

       0.202909357 seconds time elapsed
       0.761053000 seconds user
       0.002032000 seconds sys
```

### SPSC Benchmark

```
Performance counter stats for './spsc_bench 1':

     2,734,796,913      cycles
     1,415,220,603      instructions
            30,316      LLC-load-misses
         9,527,748      LLC-loads

       0.284860360 seconds time elapsed
       0.552556000 seconds user
       0.005258000 seconds sys


Performance counter stats for './spsc_bench 2':

     3,060,161,842      cycles
     1,437,785,034      instructions
            35,759      LLC-load-misses
        10,653,381      LLC-loads

       0.317709235 seconds time elapsed
       0.615324000 seconds user
       0.008249000 seconds sys
```

---

## Linux — `_printed` Timing Output

**Command used:**
```
./pingpong_bench 1_printed
./pingpong_bench 2_printed
./counter_bench 1_printed  (through 4_printed)
./spsc_bench 1_printed
./spsc_bench 2_printed
```

Run date: 2026-08-04

### Pingpong Benchmark

```
Mode 1: flag and counter on same cache line
Calibrating TSC...
TSC: 2.80799 GHz
Core 0: 800 MHz  Core 1: 800 MHz  (before)   ← stale sysfs reading, actual ~4.91 GHz
Core 0: 800 MHz  Core 1: 800 MHz  (after)
p0.1:  353 cycles (~125 ns)
p1:    355 cycles (~126 ns)
p25:   357 cycles (~127 ns)
p50:   361 cycles (~128 ns)
p75:   365 cycles (~129 ns)
p99:   382 cycles (~136 ns)
p99.9: 454 cycles (~161 ns)

Mode 2: flag and counter on separate cache lines
Calibrating TSC...
TSC: 2.80799 GHz
Core 0: 800 MHz  Core 1: 800 MHz  (before)   ← stale sysfs reading
Core 0: 800 MHz  Core 1: 800 MHz  (after)
p0.1:  240 cycles (~85 ns)
p1:    240 cycles (~85 ns)
p25:   258 cycles (~91 ns)
p50:   329 cycles (~117 ns)
p75:   331 cycles (~117 ns)
p99:   450 cycles (~160 ns)
p99.9: 525 cycles (~186 ns)
```

### Counter Benchmark

```
----------------V1 mutex start ----------------
Total time spent running: 602994040 ns
BEST: ns per operation: 60.2994
AVERAGE: ns per operation: 61.6832
----------------V1 mutex end ----------------

----------------V2 atomic start ----------------
Total time spent running: 161998770 ns
BEST: ns per operation: 16.1999
AVERAGE: ns per operation: 16.401
----------------V2 atomic end ----------------

----------------V3 sharded unpadded start ----------------
Total time spent running: 139929179 ns
BEST: ns per operation: 13.9929
AVERAGE: ns per operation: 14.1677
----------------V3 sharded unpadded end ----------------

----------------V4 sharded padded start ----------------
Total time spent running: 9545698 ns
BEST: ns per operation: 0.95457
AVERAGE: ns per operation: 1.01428
----------------V4 sharded padded end ----------------
```

### SPSC Benchmark

```
Item (shared cacheline) (best of 10):
  Throughput : 41,821,745 ops/sec
  p50        : 14 ns
  p99        : 67 ns
  p99.9      : 71 ns

PaddedItem (own cacheline) (best of 10):
  Throughput : 46,286,482 ops/sec
  p50        : 15 ns
  p99        : 47 ns
  p99.9      : 50 ns
```

---

## Linux — Computed from `perf stat`

All values derived from the raw perf stat output above. `perf stat` wraps the full binary
execution: counter benchmark runs 20 internal iterations, SPSC runs 10, pingpong runs once
(10M trips). CPU frequency is inferred per (cycles ÷ threads ÷ wall time).

### IPC (Instructions Per Cycle)

| Benchmark               | Instructions    | Cycles          | IPC   |
| ----------------------- | --------------- | --------------- | ----- |
| Pingpong Mode 1         | 704,558,492     | 13,598,193,876  | 0.052 |
| Pingpong Mode 2         | 692,502,880     | 13,344,912,027  | 0.052 |
| Counter V1 mutex        | 67,933,474,958  | 103,027,010,844 | 0.659 |
| Counter V2 atomic       | 621,150,701     | 65,706,589,314  | 0.009 |
| Counter V3 shrd unpadded| 623,506,785     | 67,600,825,816  | 0.009 |
| Counter V4 shrd padded  | 518,258,153     | 3,634,514,575   | 0.143 |
| SPSC Item               | 1,415,220,603   | 2,734,796,913   | 0.517 |
| SPSC PaddedItem         | 1,437,785,034   | 3,060,161,842   | 0.470 |

### Inferred CPU Frequency

Formula: `freq = (total_cycles / num_threads) / wall_time`

| Benchmark          | Threads | Cycles         | Wall time | Implied freq |
| ------------------ | ------- | -------------- | --------- | ------------ |
| Pingpong Mode 1    | 2       | 13,598,193,876 | 1.385 s   | **4.91 GHz** |
| Pingpong Mode 2    | 2       | 13,344,912,027 | 1.343 s   | **4.97 GHz** |
| Counter V4 padded  | 4       | 3,634,514,575  | 0.203 s   | **4.47 GHz** |
| SPSC Item          | 2       | 2,734,796,913  | 0.285 s   | **4.80 GHz** |
| SPSC PaddedItem    | 2       | 3,060,161,842  | 0.318 s   | **4.81 GHz** |

Cross-check (V4 uncontended `fetch_add`): 3.634B cycles / 4 threads / 0.203s = 4.47 GHz.
LOCK XADD latency on L1-resident uncontended line ≈ 18 cycles on Comet Lake.
18 cycles / 4.47 GHz = 4.03 ns/op per thread → 4 threads in parallel → 1.01 ns/op wall.
Measured avg: 1.014 ns/op. Consistent.

### LLC-load-miss Rate

| Benchmark               | LLC-load-misses | LLC-loads     | Miss rate |
| ----------------------- | --------------- | ------------- | --------- |
| Pingpong Mode 1         | 20,135          | 20,093,729    | 0.10%     |
| Pingpong Mode 2         | 16,752          | 20,293,034    | 0.08%     |
| Counter V1 mutex        | 7,684           | 219,050,476   | 0.004%    |
| Counter V2 atomic       | 6,264           | 262,796       | 2.38%     |
| Counter V3 shrd unpadded| 5,449           | 253,610       | 2.15%     |
| Counter V4 shrd padded  | 4,218           | 120,372       | 3.50%     |
| SPSC Item               | 30,316          | 9,527,748     | 0.32%     |
| SPSC PaddedItem         | 35,759          | 10,653,381    | 0.34%     |

Note: counter false sharing (V2/V3) is an L1/L2 phenomenon — the line bounces between
per-core caches via the ring bus. The LLC sees the traffic but the data stays cache-resident
(almost no DRAM accesses). LLC-load-misses are nearly identical across all counter variants
despite the 18× wall-time gap between V3 and V4.

### Counter Speedup Table (from `_printed` timing, 2026-08-04 run)

| Transition    | Linux (best ns/op) | Speedup |
| ------------- | ------------------ | ------- |
| V1 → V2       | 60.299 → 16.200    | 3.72×   |
| V2 → V3       | 16.200 → 13.993    | 1.16×   |
| V3 → V4       | 13.993 → 0.955     | 14.65×  |
| V1 → V4       | 60.299 → 0.955     | **63.1×** |

Note: V1 mutex shows high run-to-run variance (futex scheduling). The earlier session's
V1 best was 48.782 ns/op; this session's best is 60.299 ns/op. The V4 number is stable
across all runs (~0.95–1.01 ns/op). Use V4 speedup ratios for claims; V1 absolute numbers
should cite a range, not a point value.

---

## Linux — SPSC Head-to-Head Benchmark

Run date: 2026-08-12
Script: `scripts/spsc_queue_head_to_head/spsc_head_to_head.sh`
Targets: mine, rigtorp, moodycamel, folly, boost
Queue sizes: 64, 128, 256, 512, 1024 — 3 runs each, median recorded
Core isolation: `isolcpus=0,1,2,3 nohz_full=0,1,2,3 rcu_nocbs=0,1,2,3` (verified active)
Governor: performance, `min_perf_pct=100`
perf counters: `cycles, instructions, LLC-load-misses, l1-dcache-load-misses`
Raw output parsed to CSV: `notes/external/timing/spsc_test_data/spsc_head_to_head.csv`

### TC1 — Throughput IPC (10M push/pop iterations)

| Queue Size | Mine  | rigtorp | folly | boost | moodycamel |
| ---------- | ----- | ------- | ----- | ----- | ---------- |
| 64         | 0.312 | 0.217   | 0.416 | 0.264 | 0.099      |
| 128        | 0.411 | 0.338   | 0.485 | 0.371 | 0.096      |
| 256        | 0.403 | 0.391   | 0.361 | 0.278 | 0.101      |
| 512        | 0.441 | 0.333   | 0.534 | 0.334 | 0.110      |
| 1024       | 0.500 | 0.370   | 0.404 | 0.306 | 0.124      |

### TC1 — Instructions at N=1024 (throughput)

| Target     | Instructions |
| ---------- | ------------ |
| mine       | 298,396,542  |
| rigtorp    | 362,300,127  |
| folly      | 355,727,131  |
| boost      | 317,605,607  |
| moodycamel | 417,648,253  |

Mine uses ~16% fewer instructions than rigtorp at N=1024 — cached-index optimization reduces
cross-thread atomic loads. This is the mechanism behind mine's IPC lead at larger queue sizes.

### TC1 — L1 Misses per Kinstr (throughput)

| Queue Size | Mine  | rigtorp | folly | boost | moodycamel |
| ---------- | ----- | ------- | ----- | ----- | ---------- |
| 64         | 107.0 | 71.5    | 34.6  | 49.7  | 132.5      |
| 128        | 93.3  | 41.1    | 28.9  | 33.3  | 158.1      |
| 256        | 95.4  | 36.2    | 39.9  | 41.8  | 154.0      |
| 512        | 90.2  | 41.4    | 29.0  | 36.5  | 149.5      |
| 1024       | 77.5  | 34.1    | 34.6  | 42.5  | 109.7      |

Mine has the highest L1 miss rate per instruction despite having the fewest total instructions.
Fewer loads are issued overall, but each cross-thread load (when the cached index is stale)
is more likely to miss L1. At large queue sizes the instruction reduction wins on IPC.

### TC2 — Request/Respond IPC (10M round trips, 2 queues)

| Queue Size | Mine  | rigtorp | folly | boost | moodycamel |
| ---------- | ----- | ------- | ----- | ----- | ---------- |
| 64         | 0.081 | 0.074   | 0.064 | 0.065 | 0.099      |
| 128        | 0.073 | 0.076   | 0.063 | 0.065 | 0.103      |
| 256        | 0.070 | 0.083   | 0.062 | 0.063 | 0.102      |
| 512        | 0.078 | 0.074   | 0.063 | 0.063 | 0.106      |
| 1024       | 0.084 | 0.082   | 0.064 | 0.065 | 0.099      |

All fixed-size queues are closely matched in TC2. IPC is low across the board (~0.06–0.08)
due to the forced alternation — producer blocks until consumer responds.

### TC3 — Burst Percentiles (ns) — N=64

Workload: push 64 items → sched_yield → drain, 100K batches. Timed with rdtscp per batch.

| Target     | p25    | p50    | p75    | p99    |
| ---------- | ------ | ------ | ------ | ------ |
| mine       | 318.4  | 337.6  | 367.2  | 545.2  |
| rigtorp    | 307.3  | 324.4  | 344.0  | 401.0  |
| folly      | 370.0  | 379.3  | 403.1  | 462.6  |
| boost      | 349.7  | 375.7  | 403.8  | 466.9  |
| moodycamel | 3092.6 | 3445.5 | 3653.9 | 3959.8 |

### TC3 — Burst Percentiles (ns) — N=512

Buffer at N=512: 512 × 64 bytes = 32 KB = L1d capacity on i9-10900 (32 KB L1d per core).

| Target     | p25    | p50    | p75    | p99    |
| ---------- | ------ | ------ | ------ | ------ |
| mine       | 425.2  | 521.0  | 712.6  | 919.5  |
| rigtorp    | 305.2  | 330.5  | 355.1  | 430.9  |
| folly      | 294.5  | 318.0  | 341.5  | 427.4  |
| boost      | 350.8  | 385.3  | 420.9  | 511.8  |
| moodycamel | 2658.9 | 2943.8 | 3136.8 | 3344.4 |

### TC3 — Burst Percentiles (ns) — N=1024

Buffer at N=1024: 1024 × 64 bytes = 64 KB — overflows L1d, must use L2 (256 KB per core).

| Target     | p25    | p50    | p75    | p99    |
| ---------- | ------ | ------ | ------ | ------ |
| mine       | 375.7  | 483.3  | 745.0  | 961.2  |
| rigtorp    | 298.8  | 313.7  | 327.6  | 384.3  |
| folly      | 352.9  | 388.5  | 463.3  | 562.7  |
| boost      | 302.7  | 318.4  | 337.6  | 398.5  |
| moodycamel | 1221.9 | 1733.3 | 2184.1 | 3683.1 |

### TC3 — L1 Misses per Kinstr (burst)

| Queue Size | Mine  | rigtorp | folly | boost | moodycamel |
| ---------- | ----- | ------- | ----- | ----- | ---------- |
| 64         | 4.35  | 3.46    | 4.18  | 3.92  | 73.0       |
| 128        | 4.41  | 4.94    | 3.73  | 4.12  | 41.6       |
| 256        | 7.61  | 2.62    | 3.29  | 3.71  | 64.6       |
| 512        | 18.05 | 3.65    | 3.40  | 5.06  | 75.1       |
| 1024       | 26.47 | 2.83    | 4.78  | 4.10  | 37.8       |

Mine's L1 miss rate climbs sharply with queue size under burst patterns. At N=512 mine is
5× rigtorp's rate (18.05 vs 3.65/kinstr). The fill-then-drain pattern forces all cached
indices to go stale on every batch, amplifying the cross-thread load cost.

---

## Linux — Earlier Session Data (for reference)

From `notes/external/test_results_linux.txt`. Different run conditions (pre-TSC calibration,
different governor state). Preserved here for comparison; the 2026-08-04 data above supersedes.

### Counter (best ns/op)

| Variant             | Best ns/op |
| ------------------- | ---------- |
| V1 mutex            | 48.782     |
| V2 atomic (shared)  | 13.588     |
| V3 sharded unpadded | 13.265     |
| V4 sharded padded   | 0.947      |

### Pingpong (same cache line, `rdtscp`)

| Percentile | Cycles |
| ---------- | ------ |
| p0.1       | 256    |
| p1         | 270    |
| p25        | 273    |
| p50        | 274    |
| p75        | 276    |
| p99        | 369    |
| p99.9      | 387    |

Note: at the time these were collected, the TSC was assumed to be 3.7 GHz.
Correct conversion at 2.808 GHz: 274 cycles = 97.6 ns p50, 387 cycles = 137.8 ns p99.9.

### SPSC (best of 10)

| Mode         | Throughput    | p50   | p99   | p99.9  |
| ------------ | ------------- | ----- | ----- | ------ |
| Item         | 46,961,865/s  | 14 ns | 47 ns | 51 ns  |
| PaddedItem   | 45,962,656/s  | 14 ns | 46 ns | 72 ns  |

---

## Mac — `_printed` Timing Output

From `notes/external/test_results_mac.txt`. Timer: `mach_absolute_time`, 24 MHz (~42 ns/tick).
All latency values are tick-quantized multiples of ~41.7 ns.

### Counter Benchmark (best of 2 runs, 20 internal each)

```
----------------V1 mutex start ----------------
Total time spent running: 127455292 ns
BEST: ns per operation: 12.7455
AVERAGE: ns per operation: 13.5311
----------------V1 mutex end ----------------

----------------V2 atomic start ----------------
Total time spent running: 54338791 ns
BEST: ns per operation: 5.43388
AVERAGE: ns per operation: 5.98646
----------------V2 atomic end ----------------

----------------V3 sharded unpadded start ----------------
Total time spent running: 51705916 ns
BEST: ns per operation: 5.17059
AVERAGE: ns per operation: 5.48324
----------------V3 sharded unpadded end ----------------

----------------V4 sharded padded start ----------------
Total time spent running: 4883209 ns
BEST: ns per operation: 0.488321
AVERAGE: ns per operation: 0.492481
----------------V4 sharded padded end ----------------
```

### Counter Speedup Table (Mac)

| Transition | Mac (best ns/op)     | Speedup  |
| ---------- | -------------------- | -------- |
| V1 → V2    | 12.746 → 5.434       | 2.35×    |
| V2 → V3    | 5.434 → 5.171        | 1.05×    |
| V3 → V4    | 5.171 → 0.488        | 10.6×    |
| V1 → V4    | 12.746 → 0.488       | **26.1×** |

### Pingpong Benchmark (best of 8 runs, same cache line)

```
p0.1:  41 ns
p1:    41 ns
p25:   83 ns
p50:   83 ns
p75:   84 ns
p99:  125 ns
p99.9: 167 ns
```

Timer: `mach_absolute_time`. Tick floor ~42 ns. Values are 1-tick (41 ns), 2-tick (83 ns),
3-tick (125 ns), 4-tick (167 ns) multiples — quantization dominates latency resolution.
8 of 8 runs bit-identical at p50 and p99.

### SPSC Benchmark (best of 10)

```
Item (shared cacheline) (best of 10):
  Throughput : 30,400,776 ops/sec
  p50        : 42 ns
  p99        : 42 ns
  p99.9      : 84 ns

PaddedItem (own cacheline, alignas(128)) (best of 10):
  Throughput : 34,190,419 ops/sec
  p50        : 41 ns
  p99        : 42 ns
  p99.9      : 84 ns
```

Note: Mac uses `alignas(128)` for PaddedItem (CACHE_LINE_SIZE=128). PaddedItem wins
throughput on Mac (+12.5%) because 1024 × 128 bytes = 128 KB fits in M3 Pro P-core L2
(16 MiB per cluster). Linux uses `alignas(64)`, making the queue 64 KB — overflows Intel's
32 KB L1d, so Item wins on Linux.

---

## Mac — xctrace Hardware Counters

Source: xctrace traces collected with custom "HW_Counters" Instruments template.
Read from: Total PMC Aggregation → "counter_bench..." process row in Instruments.
Counter names are Apple Silicon PMU labels — not 1:1 with Intel `perf` names.

Note: Mac has no direct equivalent of Intel's `LLC-load-misses`. `L1D_CACHE_MISS_LD`
and `L1D_CACHE_MISS_ST` measure misses at L1d; Apple Silicon's large per-cluster shared
L2 (16 MiB) serves as the effective last-level cache for intra-cluster operations.

### Counter Benchmark

| Variant             | Cycles         | INST_ALL    | IPC   | L1D_CACHE_MISS_LD | L1D_CACHE_MISS_ST | L1D_TLB_MISS | L2_TLB_MISS_DATA |
| ------------------- | -------------- | ----------- | ----- | ----------------- | ----------------- | ------------ | ---------------- |
| V1 mutex            | 2,662,199,566  | 240,216,469 | 0.090 | 228,824,687       | 296,724,007       | 10,695       | 1,220            |
| V2 atomic (shared)  | 16,821,540,289 | 583,115,662 | 0.035 | 64,983,549        | 410,433,503       | 26,180       | 1,350            |
| V3 sharded unpadded | 14,939,848,481 | 561,025,495 | 0.038 | 59,769,250        | 391,299,663       | 37,406       | 1,854            |
| V4 sharded padded   | 993,203,519    | 425,520,102 | 0.428 | 40,502            | 12,857            | 16,509       | 1,824            |

Key derived ratios:
- V3 → V4 L1D_CACHE_MISS_ST reduction: 391,299,663 / 12,857 = **~30,435×**
- V3 → V4 L1D_CACHE_MISS_LD reduction: 59,769,250 / 40,502 = **~1,475×**
- V3 → V4 cycle reduction: 14,939,848,481 / 993,203,519 = **~15.0×**
- V2 cycles > V1 cycles on Mac (16.8B vs 2.66B): disassembly confirmed `fetch_add(relaxed)`
  emits `ldadd` (ARMv8.1 LSE), not `ldxr`/`stxr`. Four threads spinning on a contended
  `ldadd` generate more cycles than `os_unfair_lock` which sleeps losers. Opposite of
  Linux where `lock xadd` wins atomically with no retries and lower latency per handoff.

### Pingpong Benchmark

| Mode                    | Cycles        | INST_ALL       | IPC   | L1D_CACHE_MISS_LD | L1D_CACHE_MISS_ST | L1D_TLB_MISS | L2_TLB_MISS_DATA |
| ----------------------- | ------------- | -------------- | ----- | ----------------- | ----------------- | ------------ | ---------------- |
| Mode 1 (same line)      | 7,247,762,853 | 12,271,936,929 | 1.693 | 674,860,740       | 767,298           | 47,519       | 18,450           |
| Mode 2 (separate lines) | 7,851,653,567 | 13,262,640,769 | 1.689 | 622,108,964       | 1,319,796         | 785,504      | 65,741           |

Key derived ratios:
- Mode 1 is faster on Mac: 7,851,653,567 / 7,247,762,853 = **~8.3% fewer cycles**
- Mode 2 L1D_TLB_MISS vs Mode 1: 785,504 / 47,519 = **16.5× more TLB misses**
- Mode 2 L2_TLB_MISS_DATA vs Mode 1: 65,741 / 18,450 = **3.6× more**
- Mac result is opposite of Linux (where Mode 2 was 1.9% faster): Mode 2's Padded struct
  is 256 bytes on Mac (alignas(128) × 2 fields). If placed straddling a 4KB page boundary,
  flag and counter land on separate TLB pages. TLB overhead outweighs not dragging the
  counter cache line with the flag.
- IPC > 1 on both modes: Apple Silicon's wide OOO execution issues multiple spin-loop
  iterations per cycle. Not observable on x86 where IPC for the same spin-wait is 0.052.

### SPSC Benchmark

| Mode                       | Cycles        | INST_ALL      | IPC   | L1D_CACHE_MISS_LD | L1D_CACHE_MISS_ST | L1D_TLB_MISS | L2_TLB_MISS_DATA |
| -------------------------- | ------------- | ------------- | ----- | ----------------- | ----------------- | ------------ | ---------------- |
| Item (shared cacheline)    | 2,374,475,888 | 3,603,057,162 | 1.517 | 266,944,790       | 3,277,676         | 566,980      | 12,283           |
| PaddedItem (own cacheline) | 2,398,923,278 | 2,912,323,923 | 1.214 | 75,144,578        | 5,740,164         | 802,912      | 17,275           |

Key derived ratios:
- Cycle difference: 2,398,923,278 / 2,374,475,888 = **~1.0% — effectively identical**
- PaddedItem L1D_CACHE_MISS_LD vs Item: 266,944,790 / 75,144,578 = **3.55× fewer**
- PaddedItem IPC vs Item: 1.214 vs 1.517 = **20% lower IPC despite fewer cache misses**
- PaddedItem queue: 1024 × 128 bytes = **128 KiB** — exactly M3 Pro P-core L1d capacity.
  L1d pressure with other working data suppresses IPC even though queue miss count is lower.
- Item queue: 1024 × 4 bytes = 4 KB — trivially L1-resident, enables wider execution.
- `_printed` timing showed PaddedItem winning by 12.5% throughput on Mac — consistent with
  ~1% cycle difference; run-to-run variation dominates at this margin.

---

## Cross-Platform Summary

### Counter Benchmark (best ns/op)

| Variant             | Mac      | Linux    | Mac faster by |
| ------------------- | -------- | -------- | ------------- |
| V1 mutex            | 12.746   | ~49–60   | ~4–5×         |
| V2 atomic (shared)  | 5.434    | ~14–16   | ~2.6–3×       |
| V3 sharded unpadded | 5.171    | ~13–14   | ~2.5–2.7×     |
| V4 sharded padded   | 0.488    | ~0.95    | ~1.9×         |

Note: Linux V1 values vary widely across runs (futex scheduling variance). Mac V4 value
is stable. For V4 the gap narrows significantly — both platforms pay mainly the atomic
RMW cost with no contention.

### Pingpong p50 (same cache line)

| Platform | p50 (ns) | Timer            |
| -------- | -------- | ---------------- |
| Mac      | 83 ns    | mach_absolute_time (42 ns tick) |
| Linux    | 97–128 ns | rdtscp at 2.808 GHz TSC (see note) |

Linux p50 range explanation: earlier session (separate cache lines, 274 cycles at 2.808 GHz
= 97.6 ns); current session (same cache line, 361 cycles at 2.808 GHz = 128 ns). Both
are valid measurements of different configurations. See ARM64_X86_RESEARCH.md for the
Mode 1 vs Mode 2 analysis.

### SPSC Throughput

| Mode         | Mac         | Linux        |
| ------------ | ----------- | ------------ |
| Item         | 30,400,776  | 41–47M ops/s |
| PaddedItem   | 34,190,419  | 46M ops/s    |
| Winner       | PaddedItem  | Item (narrow) |

Mac/Linux winner reversal is explained by cache line size (128 vs 64 bytes) and L2 capacity
differences. See SPSC section of ARM64_X86_RESEARCH.md.
