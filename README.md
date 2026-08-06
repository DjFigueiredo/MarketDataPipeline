# MarketDataPipeline

Low-latency systems portfolio project targeting junior Performance/Systems Engineer roles at Chicago prop trading firms. Benchmarks ARM64 (Apple M3 Pro, macOS) vs x86-64 (Intel i9-10900, Ubuntu bare metal) across synchronization primitives and lock-free data structures.

---

## Repository Layout

```
bench/
  bench_utils.h          platform utilities: CACHE_LINE_SIZE, pin_thread_to_core,
                         check_cpu_governor, parse_bench_args
  counter_bench.cpp      4-variant counter contention ladder (mutex / atomic /
                         sharded unpadded / sharded padded alignas)
  pingpong_bench.cpp     acquire/release round-trip: two threads, one atomic flag,
                         Mode 1 (same cache line) vs Mode 2 (separate cache lines)
  spsc_bench.cpp         1024-slot SPSC ring buffer: Item (4B) vs PaddedItem
                         (alignas(CACHE_LINE_SIZE))

spsc/
  SpscQueue.h            lock-free SPSC ring buffer header (used by spsc_bench)

experiments/
  cas_spinlock.cpp       CAS spinlock with 4-thread fairness measurement
  broken_aba.cpp         lock-free stack ABA demo — fires deterministically
  move_semantics.cpp     Rule of 5 buffer struct
  unique_ptr.cpp         template UniquePtr<T>

notes/
  ARM64_X86_RESEARCH.md  main writeup: counter + pingpong + SPSC results,
                         mechanism analysis, Mac vs Linux hardware counter tables
  Benchmark_Findings.md  earlier counter_bench + pingpong writeup (superseded
                         by ARM64_X86_RESEARCH.md for cross-platform analysis)
  Flashcards/            Obsidian spaced-repetition cards (memory orders, CAS/ABA,
                         false sharing, SPSC)
  external/
    BENCHMARK_LOG.md     all raw timing and perf/xctrace output, derived tables
    BENCHMARK_PROCEDURE.md  step-by-step procedure for reproducible runs
    disasm_mac.txt       llvm-objdump output for all three bench binaries
    Guides/              reference guides (cache/hardware, lock-free patterns,
                         SPSC design, ARM64/x86 research guidelines)

feed/                    stub (ITCH 5.0 decoder — not started)
lob/                     stub (limit order book — not started)
pipeline/                stub (end-to-end integration — not started)
```

---

## Build

```bash
cmake -B build && cmake --build build -j

# Benchmarks
./build/bench/counter_bench           # all variants
./build/bench/counter_bench 4_printed # V4 only, verbose output
./build/bench/pingpong_bench 1_printed
./build/bench/pingpong_bench 2_printed
./build/bench/spsc_bench 1_printed
./build/bench/spsc_bench 2_printed

# ThreadSanitizer
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=thread
cmake --build build-tsan -j
```

---

## Benchmarking Methodology

All timing numbers come from pinned bare-metal runs, not CI. Methodology is documented in full in `notes/external/BENCHMARK_PROCEDURE.md`. Key points:

- **Linux:** `rdtscp` (2.808 GHz calibrated TSC), `pthread_setaffinity_np`, `isolcpus=0,1,2,3 nohz_full rcu_nocbs`, governor set to `performance`
- **macOS:** `mach_absolute_time` (24 MHz, ~42 ns/tick quantization), QoS hint only (no hard affinity), Low Power Mode disabled
- **Hardware counters:** `perf stat` on Linux, `xctrace` + Instruments on macOS
- **In-process best-of-N harness** — more stable than relaunching the binary N times

Raw output and derived tables: `notes/external/BENCHMARK_LOG.md`

Full cross-platform analysis: `notes/ARM64_X86_RESEARCH.md`

---

## CI

GitHub Actions runs `cmake -B build && cmake --build build -j` on `macos-latest` and `ubuntu-latest`. CI does not run the benchmarks — shared CI hardware is unpinned and produces meaningless latency numbers.
