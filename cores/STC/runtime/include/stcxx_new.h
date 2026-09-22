#ifndef STCXX_NEW_H
#define STCXX_NEW_H

#if !defined(__cplusplus)
# error "stcxx_new.h is a C++ header"
#endif

#include <stddef.h>

#include "stcxx_config.h"

/* Host conformance uses the host library's declarations and nothrow object. */
#if defined(STCXX_USE_SYSTEM_NEW)
# include <new>
#else
namespace std {
struct nothrow_t {
};

extern const nothrow_t nothrow;
using size_t = ::size_t;
} // namespace std

void *operator new(size_t size);
void *operator new[](size_t size);
void *operator new(size_t size, const std::nothrow_t &) noexcept;
void *operator new[](size_t size, const std::nothrow_t &) noexcept;

void operator delete(void *memory) noexcept;
void operator delete[](void *memory) noexcept;
void operator delete(void *memory, const std::nothrow_t &) noexcept;
void operator delete[](void *memory, const std::nothrow_t &) noexcept;

# if __cplusplus >= 201402L
void operator delete(void *memory, size_t size) noexcept;
void operator delete[](void *memory, size_t size) noexcept;
# endif

inline void *operator new(size_t, void *memory) noexcept
{
    return memory;
}

inline void *operator new[](size_t, void *memory) noexcept
{
    return memory;
}

inline void operator delete(void *, void *) noexcept
{
}

inline void operator delete[](void *, void *) noexcept
{
}
#endif /* STCXX_USE_SYSTEM_NEW */

#endif
