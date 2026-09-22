#ifndef STCXX_CONSOLE_H
#define STCXX_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Implement these in native C: write returns the emitted byte or EOF;
 * read returns a byte or EOF. Blocking policy belongs to the platform.
 * Native va_list and formatting remain inside the SDCC runtime. */
int stcxx_console_write(unsigned char value);
int stcxx_console_read(void);

#ifdef __cplusplus
}
#endif
#endif
