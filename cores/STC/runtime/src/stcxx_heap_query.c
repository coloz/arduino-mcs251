/* Optional query code is selected from core.lib only when referenced.
 * Heap initialization and its first integrity snapshot still run before global
 * C++ constructors, independently of whether this member is selected. */
#if !defined(__SDCC_mcs251)
# error "The STC C++ native runtime requires SDCC MCS251"
#endif

#include "stcxx_heap_private.h"

unsigned char __stcxx_heap_read_telemetry(
    stcxx_allocator_telemetry_t *telemetry)
{
    unsigned int total_free;
    unsigned int largest_free;

    if (telemetry == (stcxx_allocator_telemetry_t *)0 ||
        !stcxx_heap_telemetry_ready || !stcxx_heap_telemetry_valid ||
        !__stcxx_heap_snapshot(&total_free, &largest_free)) {
        stcxx_heap_telemetry_valid = 0u;
        return 0u;
    }
    telemetry->arena_bytes = (unsigned int)STCXX_HEAP_SIZE;
    telemetry->initial_total_free_bytes = stcxx_heap_initial_total_free;
    telemetry->current_total_free_bytes = total_free;
    telemetry->current_largest_free_block_bytes = largest_free;
    telemetry->minimum_total_free_bytes = stcxx_heap_minimum_total_free;
    telemetry->minimum_largest_free_block_bytes = stcxx_heap_minimum_largest_free;
    return 1u;
}
