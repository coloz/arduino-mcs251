#ifndef STCXX_HEAP_PRIVATE_H
#define STCXX_HEAP_PRIVATE_H

#include "../include/stcxx_allocator.h"

/* Shared native implementation details, never a C++ ABI boundary.  State
 * stays in its single eight-byte XDATA archive member; the arena object
 * continues to allocate exactly STCXX_HEAP_SIZE bytes. */
extern __xdata unsigned char __stcxx_heap_telemetry_ready_state;
extern __xdata unsigned char __stcxx_heap_telemetry_valid_state;
extern __xdata unsigned int __stcxx_heap_initial_total_free_state;
extern __xdata unsigned int __stcxx_heap_minimum_total_free_state;
extern __xdata unsigned int __stcxx_heap_minimum_largest_free_state;

#define stcxx_heap_telemetry_ready __stcxx_heap_telemetry_ready_state
#define stcxx_heap_telemetry_valid __stcxx_heap_telemetry_valid_state
#define stcxx_heap_initial_total_free __stcxx_heap_initial_total_free_state
#define stcxx_heap_minimum_total_free __stcxx_heap_minimum_total_free_state
#define stcxx_heap_minimum_largest_free __stcxx_heap_minimum_largest_free_state

unsigned char __stcxx_heap_snapshot(unsigned int *total_free,
                                    unsigned int *largest_free);

#endif
