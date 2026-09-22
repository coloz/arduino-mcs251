
#include "stcxx_allocator.h"
#include "stcxx_new.h"
#include "stcxx_runtime.h"

#if !defined(STCXX_USE_SYSTEM_NEW)
namespace std {
const nothrow_t nothrow = nothrow_t();
} // namespace std
#endif

static size_t stcxx_allocation_size(size_t size)
{
    return size == 0u ? 1u : size;
}

static void *stcxx_allocate_or_report(size_t size)
{
    /* Intentional AVR-core compatibility boundary: Arduino AVR historically
     * lets throwing new return null when NEW_TERMINATES_ON_FAILURE is absent.
     * This no-exception runtime instead preserves the C++ contract: ordinary
     * new either returns storage or enters the configured fatal OOM path.
     * Code that needs a recoverable failure must use std::nothrow. */
    size_t requested_size = stcxx_allocation_size(size);
    void *memory = stcxx_malloc(requested_size);
    if (memory == 0) {
        stcxx_out_of_memory(requested_size);
    }
    return memory;
}

void *operator new(size_t size)
{
    return stcxx_allocate_or_report(size);
}

void *operator new[](size_t size)
{
    return stcxx_allocate_or_report(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
    return stcxx_malloc(stcxx_allocation_size(size));
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
    return stcxx_malloc(stcxx_allocation_size(size));
}

void operator delete(void *memory) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}

void operator delete[](void *memory) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}

void operator delete(void *memory, const std::nothrow_t &) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}

#if __cplusplus >= 201402L
void operator delete(void *memory, size_t) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}

void operator delete[](void *memory, size_t) noexcept
{
    if (memory != 0) {
        stcxx_free(memory);
    }
}
#endif
