/* Sampling is separate from the reporting API's archive member.
 * Heap initialization and its first integrity snapshot still run before global
 * C++ constructors, independently of whether this member is selected. */
#if !defined(__SDCC_mcs251)
# error "The STC C++ native runtime requires SDCC MCS251"
#endif

#include "stcxx_heap_private.h"

void __stcxx_heap_sample(void)
{
    unsigned int total_free;
    unsigned int largest_free;

    if (!stcxx_heap_telemetry_ready || !stcxx_heap_telemetry_valid ||
        !__stcxx_heap_snapshot(&total_free, &largest_free)) {
        stcxx_heap_telemetry_valid = 0u;
        return;
    }
    if (total_free < stcxx_heap_minimum_total_free) {
        stcxx_heap_minimum_total_free = total_free;
    }
    if (largest_free < stcxx_heap_minimum_largest_free) {
        stcxx_heap_minimum_largest_free = largest_free;
    }
}
