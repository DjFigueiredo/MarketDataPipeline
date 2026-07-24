
# Memory Orders #
#flashcards/MemoryOrders

What is memory_order_relaxed
?
Relaxed operation, no synchronization or ordering constrains imposed on other reads/writes.

When is memory_order_relaxed legal
?
Any atomic operation, but only when ordering relative to other variables does not matter.

What is memory_order_consume
?
A load operation with this memory order performs `consume` on the affected memory location. No reads or writes in the current thread dependending on the value currently loaded can be reordered before this load. Other threads writing to this can will be seen by the current thread.

When is memory_order_consume legal
?
Loads only, rarely used in practice.
<!--SR:!2026-07-23,1,230-->


What is memory_order_acquire
?
A load operation with this memory order performs `acquire` on the affected memory location. No reads or wirites in the current thread can be reordered before this load. All writes in other threads that release the same variable are visible in the current thread.
<!--SR:!2026-07-25,3,250-->

When is memory_order_acquire legal
?
For loads only (Or rmw success side). Illegal on plain stores

What is memory_order_release
?
A store operation with this memory order performs the `release` on the affected memory location. No reads or writes in the current thread can be reordered after this store. All writes in current thread are visible in other threads acquiring the same atomic variable.

When is memory_order_release legal
?
Stores only (or RMW). Illegal on plain loads.

What is memory_order_acq_rel (Acquire release)
?
A read-modify-write operation with this memory order is both an `acquire` and `release`. No memory reads or writes in current thread can be reordered before the load, nor after the store.All writes in other threads that release same variable are visible before modification, and modification is visible in other threads.

When is memory_order_acq_rel legal
?
RMW operations only (CAS, fetch_add, etc.). Illegal on plain loads or stores.

What is memory_order_seq_cst()
?
A load operation with this memory order performs an `acquire`, a store performs a `release`, and read-modify-write performs both. Plus a single total order exists in which all threads observe al modifications in the same order.

When is memory_order_seq_cst legal
?
Legal anywhere - but expensive. Only when need total global order.


