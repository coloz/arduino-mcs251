#ifndef STCXX_FREESTANDING_STDBOOL_H
#define STCXX_FREESTANDING_STDBOOL_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if defined(__GNUC__) || defined(__clang__)
# include_next <stdbool.h>
#else
# error "The STC stdbool.h shim requires a compiler-provided stdbool.h"
#endif

#endif
