
# Memory Orders #
#flashcards/MemoryOrders

What is memory_order_relaxed and when is it legal?
?
No ordering constraints beyond atomicity — the operation is atomic but can be reordered freely relative to other operations. Legal on any atomic. Use only when no sequencing relative to other variables is needed (e.g., a thread reading its own counter).
<!--SR:!2026-07-28,1,230-->

What is memory_order_acquire and when is it legal?
?
A load that prevents any subsequent reads/writes from being reordered before it. Pairs with a release store — all writes the releasing thread made before its store are visible to you after this load. Legal on loads and the success side of RMW. Illegal on plain stores.
<!--SR:!2026-07-29,2,230-->

What is memory_order_release and when is it legal?
?
A store that prevents any preceding reads/writes from being reordered after it. All writes before this store become visible to any thread that acquires the same atomic. Legal on stores and RMW. Illegal on plain loads.
<!--SR:!2026-07-28,1,230-->

What is memory_order_acq_rel and when is it legal?
?
An RMW that is both an acquire (on the load half) and a release (on the store half). Nothing can be reordered across it in either direction. Legal on RMW operations only (CAS, fetch_add, exchange). Illegal on plain loads or stores.
<!--SR:!2026-07-30,3,250-->

What is memory_order_seq_cst and when is it legal?
?
Acquire + release + a single global total order that all threads observe for all seq_cst operations. On x86, emits mfence or a locked instruction on stores. Legal anywhere, but the most expensive option — use only when global ordering across multiple atomics is actually required.
<!--SR:!2026-07-28,1,230-->

# ARM64 vs x86 #
#flashcards/MemoryOrders

What is x86 TSO and why does it matter for acquire/release?
?
Total Store Order: x86 hardware guarantees stores are never reordered with prior stores, and loads are never reordered with prior loads. Acquire/release semantics are nearly free — the hardware already provides them. No extra barrier instructions are emitted for acquire loads or release stores on x86.
<!--SR:!2026-07-28,1,230-->

Why does acquire/release cost more on ARM64 than x86?
?
ARM64 has a weak memory model — loads and stores can be reordered in any direction by the CPU. Acquire/release requires explicit barrier instructions (ldar for load-acquire, stlr for store-release). x86 TSO provides the same ordering implicitly, so no extra instructions are needed there.
<!--SR:!2026-07-30,3,250-->

What instructions implement acquire/release on ARM64?
?
ldar (load-acquire) and stlr (store-release). ldar prevents any subsequent load/store from being reordered before it. stlr prevents any prior load/store from being reordered after it. x86 generates no equivalent extra instructions because TSO provides the ordering for free.
<!--SR:!2026-07-28,1,230-->

What does a release store prevent, and what does an acquire load prevent?
?
Release store: all prior reads/writes cannot move after it — it's the publication fence. Acquire load: all subsequent reads/writes cannot move before it — once you see the released value, all writes before the release are visible. Together they create a happens-before edge between the two threads.
<!--SR:!2026-07-28,1,230-->
