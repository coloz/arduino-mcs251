/*
 * Allocator telemetry state is a separate archive member so stcxx_heap.c.rel
 * continues to describe exactly the configured allocator arena.  The
 * explicit heap object references these symbols, causing this member to be
 * selected once from core.lib without becoming a second heap provider.
 */
#if !defined(__SDCC_mcs251)
# error "The STC C++ native runtime requires SDCC MCS251"
#endif



__xdata unsigned char __stcxx_heap_telemetry_ready_state;
__xdata unsigned char __stcxx_heap_telemetry_valid_state;
__xdata unsigned int __stcxx_heap_initial_total_free_state;
__xdata unsigned int __stcxx_heap_minimum_total_free_state;
__xdata unsigned int __stcxx_heap_minimum_largest_free_state;
