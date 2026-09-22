#ifndef STCXX_FREESTANDING_STDIO_H
#define STCXX_FREESTANDING_STDIO_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if !defined(__STC_CLANG_IR_ONLY__)
# if defined(__GNUC__) || defined(__clang__)
#  include_next <stdio.h>
# else
#  error "The STC stdio.h shim needs either the STC target or include_next"
# endif
#else

#include <stdarg.h>
#include <stddef.h>

/* FILE is an opaque source-compatibility type only. This SDK does not supply
 * a hosted FILE runtime; calls below fail explicitly rather than link to
 * nonexistent libc symbols. Formatting is native SDCC, not a Clang va_list. */
typedef struct __stcxx_target_FILE FILE;
typedef long fpos_t;
#define STCXX_STDIO_HAS_FILE_IO 0
#define STCXX_STDIO_HAS_SCANF 0
#define STCXX_STDIO_HAS_VA_LIST_BRIDGE 0
#define STCXX_STDIO_HAS_SNPRINTF 1
#define STCXX_STDIO_UNSUPPORTED \
    __attribute__((unavailable("STC stdio: FILE and scanf APIs are not implemented")))
#define STCXX_STDIO_VA_UNSUPPORTED \
    __attribute__((unavailable("STC stdio: Clang va_list to native SDCC ABI is not supported; use printf/sprintf/snprintf directly")))

#ifndef NULL
# ifdef __cplusplus
#  define NULL 0
# else
#  define NULL ((void *)0)
# endif
#endif
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 256
#define FILENAME_MAX 64
#define TMP_MAX 25
#define L_tmpnam 16

#ifdef __cplusplus
extern "C" {
#endif

extern FILE *stdin __attribute__((unavailable("STC has no FILE streams; use Serial")));
extern FILE *stdout __attribute__((unavailable("STC has no FILE streams; use Serial")));
extern FILE *stderr __attribute__((unavailable("STC has no FILE streams; use Serial")));

int remove(const char *path) STCXX_STDIO_UNSUPPORTED;
int rename(const char *old_path, const char *new_path) STCXX_STDIO_UNSUPPORTED;
FILE *tmpfile(void) STCXX_STDIO_UNSUPPORTED;
char *tmpnam(char *buffer) STCXX_STDIO_UNSUPPORTED;
FILE *fopen(const char *path, const char *mode) STCXX_STDIO_UNSUPPORTED;
FILE *freopen(const char *path, const char *mode, FILE *stream) STCXX_STDIO_UNSUPPORTED;
int fclose(FILE *stream) STCXX_STDIO_UNSUPPORTED;
int fflush(FILE *stream) STCXX_STDIO_UNSUPPORTED;
void setbuf(FILE *stream, char *buffer) STCXX_STDIO_UNSUPPORTED;
int setvbuf(FILE *stream, char *buffer, int mode, size_t size) STCXX_STDIO_UNSUPPORTED;

int fprintf(FILE *stream, const char *format, ...) STCXX_STDIO_UNSUPPORTED;
int fscanf(FILE *stream, const char *format, ...) STCXX_STDIO_UNSUPPORTED;
int printf(const char *format, ...) __asm__("__stcxx_printf");
int scanf(const char *format, ...) STCXX_STDIO_UNSUPPORTED;
int snprintf(char *buffer, size_t size, const char *format, ...) __asm__("__stcxx_snprintf");
int sprintf(char *buffer, const char *format, ...);
int sscanf(const char *text, const char *format, ...) STCXX_STDIO_UNSUPPORTED;
int vfprintf(FILE *stream, const char *format, va_list arguments) STCXX_STDIO_UNSUPPORTED;
int vfscanf(FILE *stream, const char *format, va_list arguments) STCXX_STDIO_UNSUPPORTED;
int vprintf(const char *format, va_list arguments) STCXX_STDIO_VA_UNSUPPORTED;
int vscanf(const char *format, va_list arguments) STCXX_STDIO_UNSUPPORTED;
int vsnprintf(char *buffer, size_t size, const char *format,
              va_list arguments) STCXX_STDIO_VA_UNSUPPORTED;
int vsprintf(char *buffer, const char *format, va_list arguments) STCXX_STDIO_VA_UNSUPPORTED;
int vsscanf(const char *text, const char *format, va_list arguments) STCXX_STDIO_UNSUPPORTED;

int fgetc(FILE *stream) STCXX_STDIO_UNSUPPORTED;
char *fgets(char *buffer, int size, FILE *stream) STCXX_STDIO_UNSUPPORTED;
int fputc(int value, FILE *stream) STCXX_STDIO_UNSUPPORTED;
int fputs(const char *text, FILE *stream) STCXX_STDIO_UNSUPPORTED;
int getc(FILE *stream) STCXX_STDIO_UNSUPPORTED;
int getchar(void) __asm__("__stcxx_getchar");
char *gets(char *buffer) __attribute__((unavailable("STC stdio: unbounded gets is unsupported; use Stream.readBytesUntil")));
int putc(int value, FILE *stream) STCXX_STDIO_UNSUPPORTED;
int putchar(int value) __asm__("__stcxx_putchar");
int puts(const char *text) __asm__("__stcxx_puts");
int ungetc(int value, FILE *stream) STCXX_STDIO_UNSUPPORTED;
size_t fread(void *buffer, size_t size, size_t count, FILE *stream) STCXX_STDIO_UNSUPPORTED;
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream) STCXX_STDIO_UNSUPPORTED;

int fgetpos(FILE *stream, fpos_t *position) STCXX_STDIO_UNSUPPORTED;
int fseek(FILE *stream, long offset, int origin) STCXX_STDIO_UNSUPPORTED;
int fsetpos(FILE *stream, const fpos_t *position) STCXX_STDIO_UNSUPPORTED;
long ftell(FILE *stream) STCXX_STDIO_UNSUPPORTED;
void rewind(FILE *stream) STCXX_STDIO_UNSUPPORTED;
void clearerr(FILE *stream) STCXX_STDIO_UNSUPPORTED;
int feof(FILE *stream) STCXX_STDIO_UNSUPPORTED;
int ferror(FILE *stream) STCXX_STDIO_UNSUPPORTED;
void perror(const char *text) STCXX_STDIO_UNSUPPORTED;

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef STCXX_STDIO_UNSUPPORTED
#undef STCXX_STDIO_VA_UNSUPPORTED
#endif /* STC target */
#endif
