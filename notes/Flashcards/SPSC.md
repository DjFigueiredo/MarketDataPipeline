# SPSC Queue #
#flashcards/SPSC

Why must SPSC capacity be a power of 2?
?
So wraparound uses a bitmask: `buf_[idx & (Capacity-1)]` instead of `buf_[idx % Capacity]`. Modulo on a non-power-of-2 is a hardware division — tens of cycles. The bitmasked AND is a single instruction. Enforced at compile time with `static_assert((Capacity & (Capacity-1)) == 0, ...)`.

Why are SPSC indices free-running (monotonic)?
?
Indices increment forever and never reset. Wraparound only happens on buffer access via the bitmask. This makes full/empty unambiguous without a sentinel slot or extra flag: empty is `head == tail`, full is `tail - head == Capacity`. If indices wrapped on overflow you'd need to sacrifice a slot or track a separate flag to tell the two apart.

What are the full and empty conditions for SPSC?
?
Empty: `head_ == tail_` — nothing to read. Full: `tail_ - head_ == Capacity` — producer is exactly one lap ahead. Both conditions work cleanly only because indices are monotonically increasing.

What memory orders go on try_push and why?
?
1. `tail_.load(relaxed)` — producer is its only writer, no sync needed. 2. `head_.load(acquire)` — must see consumer's latest progress to check fullness correctly. 3. `buf_[t & mask] = item` — plain store, ordered by the release below. 4. `tail_.store(t+1, release)` — publication fence: the consumer cannot observe the slot data until it observes the updated tail.

What memory orders go on try_pop and why?
?
1. `head_.load(relaxed)` — consumer is its only writer. 2. `tail_.load(acquire)` — pairs with producer's release; once you see the new tail, the slot write is guaranteed visible. 3. `out = buf_[h & mask]` — plain load, already ordered by the acquire above. 4. `head_.store(h+1, release)` — tells producer the slot is free; release ensures the buf_ read finished before the index advances.

What breaks if the release on tail_.store becomes relaxed?
?
The consumer can observe the updated tail_ (new index) before it sees the element written into buf_. It reads garbage from the slot. The release is the publication fence — without it there is no ordering guarantee between the slot write and the index update.

Why is SPSC simpler than MPMC?
?
Each SPSC index has exactly one writer. With multiple producers, two threads could load the same tail_ and race to claim the same slot — a plain load-then-store is not safe. MPMC needs a CAS loop (or per-slot sequence numbers as in Vyukov's queue) to atomically claim slots. SPSC's simplicity is a direct consequence of the single-writer-per-index constraint.

Why does SPSC not have ABA issues?
?
SPSC uses no CAS — correctness relies on acquire/release ordering of monotonic indices. ABA requires a stale CAS to commit on a recycled address; with no CAS in SPSC, there is no mechanism for ABA to occur.
