
#include "stcxx_allocator.h"

#if !defined(STCXX_CUSTOM_ALLOCATOR)

#include "stcxx_libc.h"

extern "C" void __stcxx_heap_sample(void);
extern "C" uint8_t __stcxx_heap_read_telemetry(
    stcxx_allocator_telemetry_t *telemetry);

static_assert(sizeof(stcxx_allocator_telemetry_t) == 12u,
              "allocator telemetry ABI must be six packed uint16_t fields");
static_assert(offsetof(stcxx_allocator_telemetry_t, arena_bytes) == 0u,
              "allocator telemetry arena offset differs");
static_assert(offsetof(stcxx_allocator_telemetry_t,
                       minimum_largest_free_block_bytes) == 10u,
              "allocator telemetry final field offset differs");

extern "C" void *stcxx_malloc(size_t size)
{
    void *memory = malloc(size);
    if (memory != 0) {
        __stcxx_heap_sample();
    }
    return memory;
}

extern "C" void *stcxx_realloc(void *memory, size_t size)
{
    void *result = realloc(memory, size);
    if (result != 0) {
        __stcxx_heap_sample();
    }
    return result;
}

extern "C" void stcxx_free(void *memory)
{
    free(memory);
}

extern "C" uint8_t stcxx_allocator_read_telemetry(
    stcxx_allocator_telemetry_t *telemetry)
{
    return __stcxx_heap_read_telemetry(telemetry);
}

#endif
