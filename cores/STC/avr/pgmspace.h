#ifndef STC_CORE_AVR_PGMSPACE_FORWARD_H
#define STC_CORE_AVR_PGMSPACE_FORWARD_H

/*
 * A number of architecture-neutral Arduino libraries still include the
 * historical AVR spelling unconditionally for every 8-bit Arduino target.
 * Keep that source-compatible entry point without advertising AVR machine
 * macros or changing the STC address-space contract: the canonical STC
 * implementation remains cores/STC/pgmspace.h.
 */
#include <stdio.h>
#include <string.h>

#include "../pgmspace.h"

/* AVR-libc exposes these spellings from avr/pgmspace.h.  Under the current
 * STCXX data-space fallback, PROGMEM data is ordinary addressable data, so the
 * corresponding libc entry points are the exact operation rather than an
 * emulation of a Harvard flash access. */
#if STCXX_PGMSPACE_DATA_FALLBACK
# ifndef _BV
#  define _BV(bit_number) (1U << (bit_number))
# endif
# ifndef strlen_P
#  define strlen_P(text) stcxx_strlen_P(text)
# endif
# ifndef memcpy_P
#  define memcpy_P(destination, source, count) \
    memcpy((destination), (source), (count))
# endif
# ifndef memcmp_P
#  define memcmp_P(left, right, count) memcmp((left), (right), (count))
# endif
# ifndef strcpy_P
#  define strcpy_P(destination, source) strcpy((destination), (source))
# endif
# ifndef strncpy_P
#  define strncpy_P(destination, source, count) \
    strncpy((destination), (source), (count))
# endif
# ifndef strcmp_P
#  define strcmp_P(left, right) strcmp((left), (right))
# endif
# ifndef strncmp_P
#  define strncmp_P(left, right, count) strncmp((left), (right), (count))
# endif
# ifndef printf_P
#  define printf_P printf
# endif
# ifndef sprintf_P
#  define sprintf_P sprintf
# endif
# ifndef snprintf_P
#  define snprintf_P snprintf
# endif
#endif

#endif
