/* SPDX-License-Identifier: MIT
 * Native-SDCC formatting boundary: va_list never crosses into Clang IR.
 * Only the C++ stdio facade selects these prefixed symbols. The embedding
 * platform supplies console hooks; this runtime has no Arduino dependency.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../include/stcxx_console.h"

typedef struct {
    char *buffer;
    size_t remaining;
} STCXXFormatBuffer;

static void stcxx_format_buffer(char value, void *opaque) __reentrant
{
    STCXXFormatBuffer *state = (STCXXFormatBuffer *)opaque;
    if (state->remaining > 1u) {
        *state->buffer++ = value;
        --state->remaining;
    }
}

int __stcxx_snprintf(char *buffer, size_t size, const char *format, ...)
{
    STCXXFormatBuffer state;
    va_list arguments;
    int result;
    if (format == NULL || (size != 0u && buffer == NULL)) {
        return EOF;
    }
    state.buffer = buffer;
    state.remaining = size;
    va_start(arguments, format);
    result = _print_format(stcxx_format_buffer, &state, format, arguments);
    va_end(arguments);
    if (size != 0u) {
        *state.buffer = '\0';
    }
    return result;
}

static void stcxx_format_uart(char value, void *opaque) __reentrant
{
    unsigned char *failed = (unsigned char *)opaque;
    if (stcxx_console_write((unsigned char)value) != (int)(unsigned char)value) {
        *failed = 1u;
    }
}

int __stcxx_printf(const char *format, ...)
{
    va_list arguments;
    unsigned char failed = 0u;
    int result;
    if (format == NULL) {
        return EOF;
    }
    va_start(arguments, format);
    result = _print_format(stcxx_format_uart, &failed, format, arguments);
    va_end(arguments);
    return failed ? EOF : result;
}

int __stcxx_putchar(int value)
{
    unsigned char character = (unsigned char)value;
    return stcxx_console_write(character) == (int)character ? (int)character : EOF;
}

int __stcxx_puts(const char *text)
{
    if (text == NULL) {
        return EOF;
    }
    while (*text != '\0') {
        if (__stcxx_putchar((unsigned char)*text++) == EOF) {
            return EOF;
        }
    }
    return __stcxx_putchar('\n') == EOF ? EOF : 0;
}

int __stcxx_getchar(void)
{
    return stcxx_console_read();
}
