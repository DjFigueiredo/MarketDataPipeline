# MarketDataPipeline

Low-latency systems portfolio project targeting junior Performance/Systems Engineer roles at Chicago prop trading firms. Benchmarks ARM64 (Apple M3 Pro, macOS) vs x86-64 (Intel i9-10900, Ubuntu bare metal) across synchronization primitives and lock-free data structures.

---

## Repository Layout

```
bench/
  bench_utils.h            platform utilities: CACHE_LINE_SIZE, pin_thread_to_core,
                           check_cpu_governor, parse_bench_args
  counter_bench.cpp        4-variant counter contention ladder (mutex / atomic /
                           sharded unpadded / sharded padded alignas)
  pingpong_bench.cpp       acquire/release round-trip: two threads, one atomic flag,
                           Mode 1 (same cache line) vs Mode 2 (separate cache lines)
  spsc_bench.cpp           1024-slot SPSC ring buffer: Item (4B) vs PaddedItem
                           (alignas(CACHE_LINE_SIZE))
  spsc_head_to_head.cpp    head-to-head throughput/latency: this repo's SpscQueue vs
                           rigtorp / moodycamel / folly / boost SPSC queues

spsc/
  SpscQueue.h              lock-free SPSC ring buffer header (used by spsc_bench,
                           spsc_head_to_head)

experiments/
  cas_spinlock.cpp         CAS spinlock with 4-thread fairness measurement
  broken_aba.cpp           lock-free stack ABA demo — fires deterministically
  move_semantics.cpp       Rule of 5 buffer struct
  unique_ptr.cpp           template UniquePtr<T>

scripts/
  spsc_queue_head_to_head/ driver script + plotting for spsc_head_to_head across queue
                           implementations and sizes; generated CSVs/plots are gitignored

notes/
  LinkedIn/                published write-ups: ARM64 vs x86 Synchronization,
                           SPSC Head-to-Head (.md + .pdf)
  flashcards/              Obsidian spaced-repetition cards (memory orders, CAS/ABA,
                           false sharing, SPSC)
  Designs/                 paper designs, promoted here once finished and reviewed —
                           see notes/Designs/README.md for the promotion rule
  external/                gitignored — raw logs, Claude-authored working docs, in-progress/
                           unreviewed design docs, personal study material, superseded drafts

feed/                      stub (ITCH 5.0 decoder — not started)
lob/                       stub (limit order book — not started)
pipeline/                  stub (end-to-end integration — not started)

third_party/                gitignored — vendored SPSC queues used only by
                           spsc_head_to_head (rigtorp, readerwriterqueue/moodycamel,
                           folly); not committed, pulled in locally as needed
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

All timing numbers come from pinned bare-metal runs, not CI. Key points:

- **Linux:** `rdtscp` (2.808 GHz calibrated TSC), `pthread_setaffinity_np`, `isolcpus=0,1,2,3 nohz_full rcu_nocbs`, governor set to `performance`
- **macOS:** `mach_absolute_time` (24 MHz, ~42 ns/tick quantization), QoS hint only (no hard affinity), Low Power Mode disabled
- **Hardware counters:** `perf stat` on Linux, `xctrace` + Instruments on macOS
- **In-process best-of-N harness** — more stable than relaunching the binary N times

Full write-ups: `notes/LinkedIn/ARM64 vs X86 Synchronization.md` and `notes/LinkedIn/SPSC_Head_to_Head.md`. Raw output and derived tables are kept locally in `notes/external/logs/` (gitignored, not published).

---

## CI

GitHub Actions runs `cmake -B build && cmake --build build -j` on `macos-latest` and `ubuntu-latest`. CI does not run the benchmarks — shared CI hardware is unpinned and produces meaningless latency numbers.
