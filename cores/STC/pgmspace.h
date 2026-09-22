#ifndef STCXX_PGMSPACE_H
#define STCXX_PGMSPACE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "stcxx_config.h"

class __FlashStringHelper;

#if STCXX_FLASH_STRINGS

#define STCXX_PGMSPACE_DATA_FALLBACK 0
#define STCXX_PROGMEM_IS_DATA 0

typedef STCXX_PGM_P PGM_P;
typedef STCXX_PGM_VOID_P PGM_VOID_P;

#ifndef PROGMEM
# define PROGMEM STCXX_PROGMEM
#endif
#ifndef PSTR
# define PSTR(string_literal) \
    (reinterpret_cast<PGM_P>(string_literal))
#endif
#ifndef F
# define F(string_literal) \
    (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))
#endif

#ifndef pgm_read_byte
static inline uint8_t pgm_read_byte(PGM_VOID_P address)
{
    return (uint8_t)STCXX_FLASH_READ_BYTE(address);
}
#endif

static inline size_t stcxx_strlen_P(PGM_P text)
{
    size_t length = 0u;
    if (text == 0) {
        return 0u;
    }
    while (STCXX_FLASH_READ_BYTE(text + length) != 0u) {
        ++length;
    }
    return length;
}

#endif /* STCXX_FLASH_STRINGS */

#if !STCXX_FLASH_STRINGS

/*
 * Compatibility-only fallback for the current generic-pointer bridge.
 * These definitions deliberately do not claim a Harvard code address space:
 * literals and PROGMEM objects remain ordinary addressable data and therefore
 * do not save RAM.  They keep conventional Arduino libraries source-compatible
 * until a backend supplies STCXX_FLASH_READ_BYTE and an honest code qualifier.
 * F() still returns Arduino's canonical helper-pointer type so overload
 * resolution remains portable; String and Print deliberately reinterpret that
 * helper as an ordinary data pointer while this fallback is selected.
 */
#define STCXX_PGMSPACE_DATA_FALLBACK 1
#define STCXX_PROGMEM_IS_DATA 1

typedef const char *PGM_P;
typedef const void *PGM_VOID_P;

#ifndef PROGMEM
# define PROGMEM
#endif
#ifndef PSTR
# define PSTR(string_literal) (string_literal)
#endif
#ifndef F
# define F(string_literal) \
    (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))
#endif

#ifndef pgm_read_byte
static inline uint8_t pgm_read_byte(PGM_VOID_P address)
{
    return *static_cast<const uint8_t *>(address);
}
#endif

#ifndef pgm_read_word
static inline uint16_t pgm_read_word(PGM_VOID_P address)
{
    return *static_cast<const uint16_t *>(address);
}
#endif

#ifndef pgm_read_dword
static inline uint32_t pgm_read_dword(PGM_VOID_P address)
{
    return *static_cast<const uint32_t *>(address);
}
#endif

#ifndef pgm_read_ptr
static inline const void *pgm_read_ptr(PGM_VOID_P address)
{
    return *static_cast<const void *const *>(address);
}
#endif

#define pgm_read_byte_near(address)  pgm_read_byte(address)
#define pgm_read_byte_far(address)   pgm_read_byte(address)
#define pgm_read_word_near(address)  pgm_read_word(address)
#define pgm_read_word_far(address)   pgm_read_word(address)
#define pgm_read_dword_near(address) pgm_read_dword(address)
#define pgm_read_dword_far(address)  pgm_read_dword(address)
#define pgm_read_ptr_near(address)   pgm_read_ptr(address)
#define pgm_read_ptr_far(address)    pgm_read_ptr(address)

static inline size_t stcxx_strlen_P(PGM_P text)
{
    size_t length = 0u;
    if (text != 0) {
        while (text[length] != '\0') {
            ++length;
        }
    }
    return length;
}

#endif /* data fallback */

/* These must also be visible through Arduino.h / <pgmspace.h>, not only
 * the historical <avr/pgmspace.h> forwarding header. They are ordinary
 * data-space operations in this profile, with no promise of RAM savings. */
#if STCXX_PGMSPACE_DATA_FALLBACK
# ifndef strlen_P
#  define strlen_P(text) stcxx_strlen_P(text)
# endif
# ifndef memcpy_P
#  define memcpy_P(destination, source, count) memcpy((destination), (source), (count))
# endif
# ifndef memcmp_P
#  define memcmp_P(left, right, count) memcmp((left), (right), (count))
# endif
# ifndef strcpy_P
#  define strcpy_P(destination, source) strcpy((destination), (source))
# endif
# ifndef strncpy_P
#  define strncpy_P(destination, source, count) strncpy((destination), (source), (count))
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
