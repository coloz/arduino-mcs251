#ifndef STCXX_FREESTANDING_STDINT_H
#define STCXX_FREESTANDING_STDINT_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if defined(__GNUC__) || defined(__clang__)
# include_next <stdint.h>
#else
# error "The STC stdint.h shim requires a compiler-provided stdint.h"
#endif

#endif
