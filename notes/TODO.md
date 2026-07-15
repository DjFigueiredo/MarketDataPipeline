#todo #tracking

# TODO

Priority markers use the Obsidian **Tasks** plugin convention (🔺 highest → 🔽 lowest) — install the plugin and this list becomes sortable/filterable automatically.

## 🔺 Highest — active

- [ ] 🔺 **Write up and explain the percentiles from BOTH `counter_bench` (V1-V4) and `pingpong_bench` in `notes/`.** Not just the raw numbers — explain _why_ each shape looks the way it does: counter_bench's mutex-vs-atomic-vs-sharded story (11x, then ~27x from padding alone, both traced to real cache-line addresses), and pingpong's flat-body/sharp-tail distribution (p1-p75 essentially flat at ~83-125ns, p99/p99.9 pulling away sharply — busy-spin exposure to OS scheduling). Include the open question on what's driving pingpong's p0.1=42ns floor (true hardware cache-bounce cost vs. spin-poll variance — currently unresolved, say so rather than guess).
- [x] 🔺 Ping-pong benchmark, flag-only variant (`relaxed` only, no payload/acquire-release) — build alongside the existing acquire/release version for the ARM-vs-x86 ordering-cost comparison. Same 10M-sample count so the two distributions are directly comparable. ✅ 2026-07-14
- [ ] 🔺 CAS spinlock in `experiments/` (your code)
- [ ] 🔺 Deliberately-broken ABA demo in `experiments/` — use `std::this_thread::sleep_for` to force the vulnerable interleaving on demand
- [ ] 🔺 MESI/MSI cache coherency — rewrite from scratch, no reference material, into `notes/`. Paste for an accuracy check afterward.

## ⏫ High

- [ ] ⏫ Guard `pthread_set_qos_class_self_np` calls with `#if defined(__APPLE__)` in `counter_bench.cpp` — CI (`ubuntu-latest`) is currently broken without this
- [ ] ⏫ `pingpong_bench.cpp` cleanup: missing `#include <algorithm>`, `"ns/n"` → `"ns\n"` typo, `std::vector<long long>` → `std::vector<int64_t>`

## 🔼 Medium

- [ ] 🔼 Set up LLDB thread debugging (VSCode + CodeLLDB, or terminal LLDB) — use during the CAS/ABA work via `watchpoint set variable`
- [ ] 🔼 Confirm `THREADING_WARMUP_FINDINGS.md` is actually in `notes/` and `git add`ed
- [ ] 🔼 Initialize `elapsed_ns_min` in `counter_bench.cpp`'s `run_and_report` (currently uninitialized; safe today only because the loop always runs at least once)
- [ ] 🔼 `run_and_report`'s hardcoded `TOTAL_INCREMENTS` divisor — never actually bit anything since pingpong built its own separate harness, but still live if that function is ever reused for a different op count

## 🔽 Low — cosmetic, whenever

- [ ] 🔽 Rename `run_sharded_unpadded_variant` — deliberately forces full colocation now, name no longer matches what it tests
- [ ] 🔽 Reconsider QoS `std::cerr` print placement in `counter_bench.cpp` — inside the timed region, fine since it's never fired
- [ ] 🔽 `percentile()` in `pingpong_bench.cpp` has an unguarded out-of-bounds case at `percentile(1.0)` — not currently called with 1.0
- [ ] 🔽 Sign-type consistency: `counter`/`NUM_ROUND_TRIPS` in `pingpong_bench.cpp` are plain `int`, inconsistent with the `int64_t` discipline used elsewhere in the file

## 📥 Backlog — no assigned day

- [ ] FrogRiverOne (Codility Lesson 4, verified real) — pull into any future Wed drill slot
- [ ] Confirm false-sharing numbers on Linux/x86 CI once available
- [ ] Instruments CPU Counters template — see real hardware cache-coherency counters instead of only inferring from timing (could help resolve the pingpong p0.1 question)