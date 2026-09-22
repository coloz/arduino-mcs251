#include "Arduino.h"
#include "../runtime/include/stcxx_console.h"
#include <stdio.h>

/* Preserve Arduino's UART1 console and cooperative blocking getchar policy.
 * The shared SDK runtime only knows the native C hooks below. */
int stcxx_console_write(unsigned char value)
{
    return Serial_write(value) == 1u ? (int)value : EOF;
}

int stcxx_console_read(void)
{
    int value;
    do {
        value = Serial_read();
        if (value < 0) yield();
    } while (value < 0);
    return value;
}
