#ifndef STCXX_CONFIG_H
#define STCXX_CONFIG_H

#include <limits.h>
#include <stddef.h>

/*
 * Public configuration contract for the experimental STC C++ core.
 *
 * The class layer is deliberately freestanding and requires C++11 or newer.
 * Production builds must use -fno-exceptions -fno-rtti.  Those switches are
 * driver policy, so a header cannot reliably prove that every compiler used
 * them; the host conformance runner does compile with both switches.
 */
#if !defined(__cplusplus) || (__cplusplus < 201103L)
# error "The STC Arduino C++ core requires C++11 or newer"
#endif

#define STCXX_API_VERSION 1
#define STCXX_EXCEPTIONS_SUPPORTED 0
#define STCXX_RTTI_SUPPORTED 0

#ifndef STCXX_ENFORCE_NO_EXCEPTIONS_RTTI
# define STCXX_ENFORCE_NO_EXCEPTIONS_RTTI 0
#endif
#ifndef STCXX_TARGET_ABI
# define STCXX_TARGET_ABI 0
#endif
#if defined(STCXX_TARGET_MCS51) && STCXX_TARGET_MCS51
# error "MCS51 C++ support has been removed"
#endif
#ifndef STCXX_TARGET_MCS251
# define STCXX_TARGET_MCS251 0
#endif
#ifndef STCXX_TARGET_ENDIAN_LITTLE
# define STCXX_TARGET_ENDIAN_LITTLE 0
#endif
#ifndef STCXX_TARGET_ENDIAN_BIG
# define STCXX_TARGET_ENDIAN_BIG 0
#endif
#ifndef STCXX_HOST_TEST
# define STCXX_HOST_TEST 0
#endif

#if ((STCXX_ENFORCE_NO_EXCEPTIONS_RTTI != 0) && \
     (STCXX_ENFORCE_NO_EXCEPTIONS_RTTI != 1)) || \
    ((STCXX_TARGET_ABI != 0) && (STCXX_TARGET_ABI != 1)) || \
    ((STCXX_TARGET_MCS251 != 0) && (STCXX_TARGET_MCS251 != 1)) || \
    ((STCXX_TARGET_ENDIAN_LITTLE != 0) && \
     (STCXX_TARGET_ENDIAN_LITTLE != 1)) || \
    ((STCXX_TARGET_ENDIAN_BIG != 0) && (STCXX_TARGET_ENDIAN_BIG != 1)) || \
    ((STCXX_HOST_TEST != 0) && (STCXX_HOST_TEST != 1))
# error "STC C++ configuration switches must be numeric 0 or 1"
#endif

#if STCXX_ENFORCE_NO_EXCEPTIONS_RTTI
# if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#  error "The STC C++ core must be compiled with exceptions disabled"
# endif
# if defined(__GXX_RTTI) || defined(_CPPRTTI)
#  error "The STC C++ core must be compiled with RTTI disabled"
# endif
#endif

#if defined(double)
# error "The STC C++ ABI forbids defining the keyword double as a macro"
#endif

#if STCXX_TARGET_ABI
static_assert(CHAR_BIT == 8, "stc-arduino-cxx-v1 requires 8-bit bytes");
static_assert(sizeof(bool) == 1u, "stc-arduino-cxx-v1 requires 8-bit bool");
static_assert(sizeof(char) == 1u, "stc-arduino-cxx-v1 requires 8-bit char");
static_assert(static_cast<char>(-1) > 0,
              "stc-arduino-cxx-v1 requires unsigned plain char");
static_assert(sizeof(short) == 2u,
              "stc-arduino-cxx-v1 requires 16-bit short");
static_assert(sizeof(int) == 2u, "stc-arduino-cxx-v1 requires 16-bit int");
static_assert(sizeof(long) == 4u, "stc-arduino-cxx-v1 requires 32-bit long");
static_assert(sizeof(long long) == 8u,
              "stc-arduino-cxx-v1 requires 64-bit long long");
static_assert(sizeof(float) == 4u, "stc-arduino-cxx-v1 requires 32-bit float");
static_assert(sizeof(double) == 4u, "stc-arduino-cxx-v1 requires 32-bit double");
static_assert(sizeof(long double) == 4u,
              "stc-arduino-cxx-v1 requires 32-bit long double");
static_assert(alignof(bool) == 1u && alignof(char) == 1u &&
                  alignof(short) == 1u && alignof(int) == 1u &&
                  alignof(long) == 1u && alignof(long long) == 1u &&
                  alignof(float) == 1u && alignof(double) == 1u &&
                  alignof(long double) == 1u,
              "stc-arduino-cxx-v1 scalar types must be byte aligned");
static_assert(sizeof(void *) == 3u,
              "stc-arduino-cxx-v1 requires 24-bit generic pointers");
static_assert(alignof(void *) == 1u,
              "stc-arduino-cxx-v1 requires byte-aligned pointers");
static_assert(sizeof(ptrdiff_t) == 4u,
              "stc-arduino-cxx-v1 requires 32-bit ptrdiff_t");
static_assert(alignof(ptrdiff_t) == 1u,
              "stc-arduino-cxx-v1 requires byte-aligned ptrdiff_t");
# if STCXX_TARGET_MCS251
#  if !STCXX_TARGET_ENDIAN_BIG || STCXX_TARGET_ENDIAN_LITTLE
#   error "MCS251 stc-arduino-cxx-v1 requires explicit big-endian TargetInfo"
#  endif
static_assert(sizeof(size_t) == 4u,
              "MCS251 stc-arduino-cxx-v1 requires 32-bit size_t");
static_assert(alignof(size_t) == 1u,
              "MCS251 stc-arduino-cxx-v1 requires byte-aligned size_t");
static_assert(sizeof(void (*)(void)) == 3u,
              "MCS251 stc-arduino-cxx-v1 requires 24-bit function pointers");
# else
#  error "STCXX_TARGET_ABI requires STCXX_TARGET_MCS251"
# endif
#endif

/*
 * Flash strings are enabled only when a backend supplies an honest code-space
 * read operation.  Host tests opt in because code and data pointers are the
 * same there.  A target backend may define:
 *
 *   STCXX_FLASH_STRINGS=1
 *   STCXX_PGM_P=<the target's code-pointer type>
 *   STCXX_PGM_VOID_P=<the target's untyped code-pointer type>
 *   STCXX_FLASH_READ_BYTE(pointer)=<code-space byte load>
 *   STCXX_PROGMEM=<code-space object qualifier>
 *
 * Leaving this disabled is intentional: silently treating a code pointer as
 * an XDATA pointer would compile but corrupt data on an 8051 target.
 */
#ifndef STCXX_FLASH_STRINGS
# if STCXX_HOST_TEST
#  define STCXX_FLASH_STRINGS 1
# else
#  define STCXX_FLASH_STRINGS 0
# endif
#endif

#if (STCXX_FLASH_STRINGS != 0) && (STCXX_FLASH_STRINGS != 1)
# error "STCXX_FLASH_STRINGS must be numeric 0 or 1"
#endif

#if STCXX_FLASH_STRINGS
# ifndef STCXX_PGM_P
#  define STCXX_PGM_P const char *
# endif
# ifndef STCXX_PGM_VOID_P
#  define STCXX_PGM_VOID_P const void *
# endif
# ifndef STCXX_FLASH_READ_BYTE
#  if STCXX_HOST_TEST
#   define STCXX_FLASH_READ_BYTE(pointer) \
        (*reinterpret_cast<const unsigned char *>(pointer))
#  else
#   error "STCXX_FLASH_READ_BYTE is required when target flash strings are enabled"
#  endif
# endif
# ifndef STCXX_PROGMEM
#  if STCXX_HOST_TEST
#   define STCXX_PROGMEM
#  else
#   error "STCXX_PROGMEM is required when target flash strings are enabled"
#  endif
# endif
#endif

#endif
