/*
 * The stock SDCC malloc archive supplies a 1024-byte XDATA heap.  That is
 * smaller than a single ArduinoJson 7 pool on the 24-bit MCS251 ABI (3328
 * bytes), so C++ profiles provide an explicit board-sized heap instead.  The
 * allocator arena retains a single-provider/link-audit contract.
 *
 * This translation unit is pulled out of the core archive by the call from
 * main.c.  Defining the two SDCC heap symbols before libc is scanned prevents
 * the archive's _heap.rel fallback from being selected.
 */
#if !defined(__SDCC_mcs251)
# error "The STC C++ native runtime requires SDCC MCS251"
#endif


#include <stddef.h>

#include "stcxx_heap_private.h"


#if !defined(STCXX_HEAP_SIZE)
#error "A board C++ profile must define STCXX_HEAP_SIZE."
#endif

#if defined(STCXX_MCS251_CONSTRAINED_HEAP) && \
    STCXX_MCS251_CONSTRAINED_HEAP != 1
#error "STCXX_MCS251_CONSTRAINED_HEAP must be exactly 1 when defined."
#elif defined(STCXX_MCS251_CONSTRAINED_HEAP) && \
      !defined(__SDCC_mcs251)
#error "The constrained STCXX heap contract is only valid on MCS251."
#elif defined(STCXX_MCS251_CONSTRAINED_HEAP) && \
      STCXX_HEAP_SIZE != 3584UL
#error "The constrained MCS251 STCXX heap must be exactly 3584 bytes."
#elif defined(__SDCC_mcs251) && \
      !defined(STCXX_MCS251_CONSTRAINED_HEAP) && \
      STCXX_HEAP_SIZE < 4096UL
#error "MCS251 STCXX_HEAP_SIZE must fit one ArduinoJson 7 pool."
#endif

#if STCXX_HEAP_SIZE > 32768UL
#error "STCXX_HEAP_SIZE exceeds the qualified SDCC MCS251 heap bound."
#endif

__xdata unsigned char __sdcc_heap[STCXX_HEAP_SIZE];
/* Match the compiler runtime's explicit 32-bit custom-heap contract. */
const unsigned long __sdcc_heap_size32 = STCXX_HEAP_SIZE;

extern void __sdcc_heap_init(void);

typedef struct stcxx_heap_header __xdata stcxx_heap_header_t;

struct stcxx_heap_header {
    stcxx_heap_header_t *next;
    stcxx_heap_header_t *next_free;
};

#define STCXX_HEAP_POINTER_BYTES 3u

/*
 * Compile-time ABI checks for the locked SDCC device/lib/malloc.c header.
 * The typedef form remains usable with the qualified SDCC C frontend.
 */
typedef char stcxx_heap_pointer_size_must_match[
    sizeof(stcxx_heap_header_t *) == STCXX_HEAP_POINTER_BYTES ? 1 : -1];
typedef char stcxx_heap_payload_offset_must_match[
    offsetof(struct stcxx_heap_header, next_free) ==
        STCXX_HEAP_POINTER_BYTES ? 1 : -1];
typedef char stcxx_heap_header_size_must_match[
    sizeof(struct stcxx_heap_header) ==
        (2u * STCXX_HEAP_POINTER_BYTES) ? 1 : -1];
typedef char stcxx_heap_telemetry_size_must_match[
    sizeof(stcxx_allocator_telemetry_t) == 12u ? 1 : -1];
typedef char stcxx_heap_telemetry_arena_offset_must_match[
    offsetof(stcxx_allocator_telemetry_t, arena_bytes) == 0u ? 1 : -1];
typedef char stcxx_heap_telemetry_final_offset_must_match[
    offsetof(stcxx_allocator_telemetry_t,
             minimum_largest_free_block_bytes) == 10u ? 1 : -1];

extern stcxx_heap_header_t * __xdata __sdcc_heap_free;

/*
 * Match the free-list ABI in the locked SDCC device/lib/malloc.c.  Every
 * pointer must stay inside this board's explicit heap, the address-sorted
 * free list must move strictly forward, and the node limit rejects cycles
 * even if corrupt pointers happen to remain in range.
 */
unsigned char __stcxx_heap_snapshot(
    unsigned int *total_free,
    unsigned int *largest_free)
{
    unsigned char __xdata *const heap_start = &__sdcc_heap[0];
    unsigned char __xdata *const heap_end =
        &__sdcc_heap[STCXX_HEAP_SIZE - 1u];
    stcxx_heap_header_t *header = __sdcc_heap_free;
    unsigned int nodes = 0u;
    const unsigned int maximum_nodes =
        (unsigned int)(STCXX_HEAP_SIZE /
                       sizeof(struct stcxx_heap_header)) + 1u;
    const unsigned int payload_offset =
        (unsigned int)offsetof(struct stcxx_heap_header, next_free);

    *total_free = 0u;
    *largest_free = 0u;
    while (header != (stcxx_heap_header_t *)0) {
        unsigned char __xdata *address =
            (unsigned char __xdata *)header;
        unsigned char __xdata *next_address;
        stcxx_heap_header_t *next_free;
        unsigned int raw_bytes;
        unsigned int payload_bytes;

        ++nodes;
        if (nodes > maximum_nodes || address < heap_start ||
            address >= heap_end ||
            (unsigned int)(heap_end - address) <
                (unsigned int)sizeof(struct stcxx_heap_header)) {
            return 0u;
        }
        next_address = (unsigned char __xdata *)header->next;
        next_free = header->next_free;
        if (next_address <= address || next_address > heap_end ||
            (next_free != (stcxx_heap_header_t *)0 &&
             ((unsigned char __xdata *)next_free <= address ||
              (unsigned char __xdata *)next_free >= heap_end ||
              next_address > (unsigned char __xdata *)next_free))) {
            return 0u;
        }
        raw_bytes = (unsigned int)(next_address - address);
        if (raw_bytes < payload_offset) {
            return 0u;
        }
        payload_bytes = raw_bytes - payload_offset;
        *total_free += payload_bytes;
        if (payload_bytes > *largest_free) {
            *largest_free = payload_bytes;
        }
        header = next_free;
    }
    return 1u;
}

void __stcxx_heap_init(void)
{
    unsigned int total_free;
    unsigned int largest_free;

    __sdcc_heap_init();
    stcxx_heap_telemetry_ready = 1u;
    stcxx_heap_telemetry_valid =
        __stcxx_heap_snapshot(&total_free, &largest_free);
    stcxx_heap_initial_total_free = total_free;
    stcxx_heap_minimum_total_free = total_free;
    stcxx_heap_minimum_largest_free = largest_free;
}
