#ifndef STCXX_LIBC_H
#define STCXX_LIBC_H

#include <stddef.h>

#include "stcxx_config.h"

/*
 * The STC Clang frontend is freestanding and intentionally does not consume
 * host libc headers.  Only declare the small C-library surface used by the
 * class layer; the bridge emits ordinary C calls which are resolved by the
 * matching SDCC target library.  Host conformance continues to use the host's
 * own declarations and implementation.
 */
#if STCXX_TARGET_ABI

extern "C" {

void *malloc(size_t size) __asm__("__stcxx_libc_malloc");
void *realloc(void *memory, size_t size) __asm__("__stcxx_libc_realloc");
void free(void *memory);

void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size) __asm__("__stcxx_libc_memset");
int memcmp(const void *left, const void *right, size_t size);
size_t strlen(const char *text);
int strcmp(const char *left, const char *right);
char *strchr(const char *text, int value) __asm__("__stcxx_libc_strchr");
char *strstr(const char *text, const char *needle);

int isspace(int value);
int isalnum(int value);
int tolower(int value);
int toupper(int value);

long strtol(const char *text, char **end, int base);
double atof(const char *text);

} /* extern "C" */

#else

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#endif

#endif
