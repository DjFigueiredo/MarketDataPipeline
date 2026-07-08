# MarketDataPipeline

Market-data pipeline in modern C++20: binary feed decoder -> lock-free SPSC queue
-> limit order book, benchmarked end to end (p50/p99/p99.9/max).

## Layout
- bench/       latency/throughput harness + micro-benchmarks (counter contention, ping-pong)
- spsc/        single-producer single-consumer ring buffer (Week 4)
- lob/         limit order book (Week 7)
- feed/        ITCH 5.0 decoder (Week 10)
- pipeline/    end-to-end integration, links spsc + lob + feed (Week 10+)
- experiments/ spinlocks, ABA demos, throwaway probes
- notes/       written analyses of measured results

## Build
    cmake -B build && cmake --build build -j
    ./build/bench/counter_bench

    cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=thread
    cmake --build build-tsan -j

Each subdirectory becomes a CMake target as its week unlocks (uncomment in the root
CMakeLists.txt); pipeline/ links spsc, lob, and feed as libraries via target_link_libraries,
so that dependency is explicit rather than hidden in a build script.

Benchmark numbers in READMEs come from local pinned runs (methodology documented per
component) — never from CI, which runs on shared, unpinned hardware.
# MarketDataPipeline
