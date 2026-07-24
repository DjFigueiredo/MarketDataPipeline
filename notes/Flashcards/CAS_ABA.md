# CAS #
#flashcards/CAS

What does Compare-And-Swap (CAS) do atomically
?
Reads the current value at an address, compares it to an expected value, and only writes a new value if they match. If they don't match, it returns false and writes the actual current value into `expected`. The entire read-compare-write is one atomic hardware instruction — no other thread can observe it mid-operation.

What is the difference between `compare_exchange_weak` and `compare_exchange_strong`
?
`compare_exchange_weak` can spuriously fail — it returns false even when the value matches expected. This is allowed on platforms where CAS is implemented as LL/SC (ARM, RISC-V) and the reservation can be lost by a context switch or cache event. Always use it in a loop. `compare_exchange_strong` never spuriously fails but may be slower on those platforms. Use `strong` when a single attempt matters (no retry loop); use `weak` inside a loop.

Why does `compare_exchange_weak` require a loop
?
Because it can spuriously fail on LL/SC architectures even when no other thread interfered. Without a loop, a spurious failure causes you to incorrectly bail out. The canonical pattern is `while (!cas_weak(expected, desired)) { ... }` — the loop retries until success or a real conflict is detected.

What memory orders go on a successful CAS and on a failed CAS
?
Successful CAS performs a read-modify-write — use `memory_order_acq_rel` (acquire the old value, release the new one). Failed CAS only reads the current value — use `memory_order_relaxed` or `memory_order_acquire`. The failed path never writes, so release is meaningless and illegal on a plain load. Common pairing: `compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_relaxed)`.

What is the TTAS optimization and why does it help
?
Test-and-Test-and-Set: before attempting CAS, first do a plain load to check if the lock is free. Only attempt the CAS if the load says it's free. Without TTAS, every spinning thread hammers the cache line with CAS (which requires exclusive ownership), causing constant invalidations. With TTAS, spinning threads do cheap shared reads — the line stays in Shared state across all cores. Only when the flag flips do they race to CAS, reducing coherency traffic significantly.

Why do you reset `expected` after a failed CAS
?
Because `compare_exchange_weak/strong` overwrites `expected` with the actual current value on failure. If you don't reset it, the next iteration compares against the wrong value — you're now trying to CAS from whatever the current value is, which may not be what you intended. Always reset `expected` to your intended old value at the top of the retry loop.

# ABA #
#flashcards/ABA

What is the ABA problem
?
A thread reads value A from a shared location. It is preempted. Another thread changes the value A → B → A (back to A). The original thread resumes and its CAS succeeds — it sees A and thinks nothing changed — but the state of the system may have changed meaningfully in between. The CAS cannot distinguish "A because nothing happened" from "A because it went through B and back."
<!--SR:!2026-07-22,1,230-->

What are the three conditions required for ABA to fire
?
1. A CAS-based algorithm reads a pointer/value before being preempted. 2. Another thread modifies that location through at least one intermediate state and returns it to the original value — typically by freeing and reallocating a node to the same address. 3. The original thread resumes and its CAS commits the stale state, corrupting the data structure.

Why does ABA not apply to SPSC
?
SPSC uses no CAS — correctness relies entirely on acquire/release ordering of monotonically increasing indices. There is no compare-and-swap, so there is no opportunity for a stale value to be committed. ABA only applies to CAS-based structures where a pointer can be observed, freed, reallocated, and re-observed at the same address.
<!--SR:!2026-07-22,1,230-->

Name two mitigations for ABA in a lock-free stack
?
1. **Tagged pointers**: pack a version counter into the unused high bits of the pointer. Each modification increments the tag, so even if the address is reused, the full tagged value differs and CAS fails. 2. **Hazard pointers**: before dereferencing a pointer, publish it in a per-thread hazard record. The memory reclaimer checks all hazard records before freeing — a node in use is never freed and therefore never reallocated to the same address while a thread holds a reference.
