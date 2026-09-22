#ifndef STCXX_FREESTANDING_STDLIB_H
#define STCXX_FREESTANDING_STDLIB_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if !defined(__STC_CLANG_IR_ONLY__)
# if defined(__GNUC__) || defined(__clang__)
#  include_next <stdlib.h>
# else
#  error "The STC stdlib.h shim needs either the STC target or include_next"
# endif
#else

#include <stddef.h>

/* Arduino.h defines abs as a macro.  It must not rewrite this C declaration
 * when a library includes <stdlib.h> after <Arduino.h>. */
#if defined(abs)
# pragma push_macro("abs")
# undef abs
# define STCXX_STDLIB_RESTORE_ABS 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

#ifndef NULL
# ifdef __cplusplus
#  define NULL 0
# else
#  define NULL ((void *)0)
# endif
#endif
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#ifndef RAND_MAX
# define RAND_MAX 32767
#endif

/* Native allocation adapters keep the C++ boundary explicit. */

void *malloc(size_t size) __asm__("__stcxx_libc_malloc");
void *calloc(size_t count, size_t size) __asm__("__stcxx_libc_calloc");
void *realloc(void *memory, size_t size) __asm__("__stcxx_libc_realloc");
void free(void *memory);

double atof(const char *text);
int atoi(const char *text);
long atol(const char *text);
char *itoa(int value, char *buffer, int radix);
char *utoa(unsigned int value, char *buffer, int radix);
char *ltoa(long value, char *buffer, int radix);
char *ultoa(unsigned long value, char *buffer, int radix);
long long atoll(const char *text);
double strtod(const char *text, char **end);
float strtof(const char *text, char **end);
long strtol(const char *text, char **end, int base);
unsigned long strtoul(const char *text, char **end, int base);
long long strtoll(const char *text, char **end, int base);
unsigned long long strtoull(const char *text, char **end, int base);

int abs(int value);
long labs(long value);
long long llabs(long long value);
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
lldiv_t lldiv(long long numerator, long long denominator);

int rand(void);
void srand(unsigned int seed);
void abort(void);
void exit(int status);

void *bsearch(const void *key, const void *base, size_t count, size_t size,
              int (*compare)(const void *, const void *));
void qsort(void *base, size_t count, size_t size,
           int (*compare)(const void *, const void *));

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(STCXX_STDLIB_RESTORE_ABS)
# pragma pop_macro("abs")
# undef STCXX_STDLIB_RESTORE_ABS
#endif

#endif /* STC target */
#endif
