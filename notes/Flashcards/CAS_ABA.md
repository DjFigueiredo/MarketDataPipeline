# CAS #
#flashcards/CAS

What does Compare-And-Swap (CAS) do atomically?
?
Reads the current value, compares it to an expected value, and writes a new value only if they match. On failure it writes the actual current value into `expected` and returns false. The entire read-compare-write is one atomic hardware instruction.

What is the difference between compare_exchange_weak and compare_exchange_strong?
?
`weak` can spuriously fail on LL/SC architectures (ARM, RISC-V) — a context switch or cache event can drop the reservation even with no competing thread. Always use it in a retry loop. `strong` never spuriously fails but may be slower on LL/SC. Use `strong` for a single-shot attempt; `weak` inside a loop.

What memory orders go on a successful CAS and on a failed CAS?
?
Successful CAS is a read-modify-write — use `acq_rel`. Failed CAS only reads — use `relaxed` or `acquire` (never release, since no write occurred). Common pairing: `compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_relaxed)`.

What is the TTAS optimization and why does it help?
?
Test-and-Test-and-Set: spin on a plain load first; only attempt CAS when the load shows the lock is free. Without TTAS, every spinning thread issues CAS requiring exclusive cache line ownership — constant cross-core invalidations. With TTAS, spinning threads stay in Shared state on reads; traffic only spikes when the lock actually releases.

Why do you reset `expected` after a failed CAS?
?
Because `compare_exchange_*` overwrites `expected` with the actual current value on failure. If you don't reset it, the next iteration compares against whatever the current value happened to be, not your intended old value. Reset `expected` at the top of every retry loop.

# ABA #
#flashcards/ABA

What is the ABA problem?
?
A thread reads value A, gets preempted, another thread changes A → B → A. The original thread resumes and its CAS succeeds — it sees A and assumes nothing changed, but the data structure may be in a corrupted intermediate state. CAS cannot distinguish "A because nothing happened" from "A after a round-trip through B."
<!--SR:!2026-07-22,1,230-->

What are the three conditions required for ABA to fire?
?
1. A thread reads a pointer/value before being preempted. 2. Another thread cycles the value through at least one intermediate state and back — typically by freeing and reallocating a node to the same address. 3. The original thread's CAS commits on the now-stale value, corrupting the structure.

Why does ABA not apply to SPSC?
?
SPSC uses no CAS — it uses acquire/release stores and loads on monotonically increasing indices. There is no compare-and-swap, so there is no stale value to be committed. ABA only applies to CAS-based structures where a pointer can be freed and reallocated to the same address.
<!--SR:!2026-07-28,1,210-->

Name two mitigations for ABA in a lock-free stack?
?
1. **Tagged pointers**: pack a version counter into the high bits of the pointer — each modification increments the tag, so address reuse doesn't fool CAS. 2. **Hazard pointers**: before dereferencing, publish the pointer in a per-thread hazard record; the reclaimer never frees a node that appears in any hazard record, preventing the reallocation that enables ABA.
