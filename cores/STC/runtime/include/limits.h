#ifndef STCXX_FREESTANDING_LIMITS_H
#define STCXX_FREESTANDING_LIMITS_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

/* Clang's resource limits.h is target-aware and remains the single source of
 * truth for scalar widths.  This forwarding shim makes that dependency
 * explicit while the target is compiled with -nostdinc. */
#if defined(__GNUC__) || defined(__clang__)
# include_next <limits.h>
#else
# error "The STC limits.h shim requires a compiler-provided limits.h"
#endif

#endif
