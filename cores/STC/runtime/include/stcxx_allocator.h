#ifndef STCXX_ALLOCATOR_H
#define STCXX_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocator ABI used by String.  A target may provide these from its XDATA or
 * EDATA heap.  Define STCXX_CUSTOM_ALLOCATOR when replacing the defaults.
 */
void *stcxx_malloc(size_t size);
void *stcxx_realloc(void *memory, size_t size);
void stcxx_free(void *memory);

typedef struct stcxx_allocator_telemetry {
    uint16_t arena_bytes;
    uint16_t initial_total_free_bytes;
    uint16_t current_total_free_bytes;
    uint16_t current_largest_free_block_bytes;
    uint16_t minimum_total_free_bytes;
    uint16_t minimum_largest_free_block_bytes;
} stcxx_allocator_telemetry_t;

uint8_t stcxx_allocator_read_telemetry(
    stcxx_allocator_telemetry_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif
