# False Sharing #
#flashcards/FalseSharing

What is false sharing?
?
When two threads write to different variables that happen to share a cache line. Each write invalidates the entire line for the other core, forcing a reload on every access — even though the threads never touch the same data. The coherency penalty is identical to a true data race.

How do you fix false sharing?
?
Put each hot variable on its own cache line with `alignas(64)`. 64 bytes is the cache line size on x86 and ARM. This guarantees the compiler won't pack the variable with neighbors, so each core's writes hit a different line.

What is the V1→V4 story in counter_bench?
?
V1 (mutex): serialized, highest latency. V2 (single shared atomic): removes mutex overhead but all threads hammer one cache line. V3 (sharded atomics, no padding): reduces contention but shards share cache lines — false sharing costs nearly as much as V2. V4 (sharded + alignas(64)): each shard on its own line, no cross-core invalidations — fastest.

Why was the V2→V3 gap near-zero on Apple Silicon but visible on x86?
?
Hypothesis: Apple Silicon has a per-cluster shared L2. Cores in the same cluster resolve same-line RMW traffic through L2 without a full cross-core coherency bounce, softening the false-sharing penalty. On x86 each core has a private L2, so unpadded shards cause expensive inter-core invalidations. Linux x86 measurements confirmed the expected gap.

What is partial colocation and why does it matter?
?
When a struct has some fields padded and some not — e.g., the control indices are on separate lines but the data buffer shares a line with something else. Partial colocation means you fix the obvious false sharing but miss a subtler one. The lesson from counter_bench: measure with real printed cache-line addresses, don't assume layout from source order.
