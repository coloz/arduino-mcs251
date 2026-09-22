#ifndef STCXX_FREESTANDING_STDDEF_H
#define STCXX_FREESTANDING_STDDEF_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if defined(__GNUC__) || defined(__clang__)
# include_next <stddef.h>
#else
# error "The STC stddef.h shim requires a compiler-provided stddef.h"
#endif

#endif
