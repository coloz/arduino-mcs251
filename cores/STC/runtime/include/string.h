#ifndef STCXX_FREESTANDING_STRING_H
#define STCXX_FREESTANDING_STRING_H

#if !defined(__STC_CLANG_IR_ONLY__)
# if defined(__GNUC__) || defined(__clang__)
#  pragma GCC system_header
#  include_next <string.h>
# else
#  error "The STC string.h shim needs either the STC target or include_next"
# endif
#else

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size) __asm__("__stcxx_libc_memset");
int memcmp(const void *left, const void *right, size_t size);
void *memchr(const void *memory, int value, size_t size);

size_t strlen(const char *text);
size_t strnlen(const char *text, size_t maximum);
char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t size);
char *strcat(char *destination, const char *source);
char *strncat(char *destination, const char *source, size_t size);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t size);
/* Search adapters normalize int arguments to unsigned char. */

char *strchr(const char *text, int value) __asm__("__stcxx_libc_strchr");
char *strrchr(const char *text, int value) __asm__("__stcxx_libc_strrchr");
char *strstr(const char *text, const char *needle);
char *strtok(char *text, const char *delimiters);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STC target */
#endif
