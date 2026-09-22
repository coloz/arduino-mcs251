#include "Arduino.h"

#include "runtime/include/stcxx_runtime.h"
void __stcxx_heap_init(void);
#if ARDUINO_USB_CDC_ON_BOOT
void stc_cdc_boot(void) STC_REENTRANT;
#endif

/*
 * SDCC emits the interrupt vector table from declarations visible in the
 * translation unit that defines main().  The ISR implementations live in
 * separate core archive members, so declarations only at their definitions
 * are not sufficient to create the vector entries during the final link.
 */
#if STC_CORE_HAS_INT0
void stc_external0_isr(void) __interrupt (0);
#endif

void stc_timer0_isr(void) __interrupt (1);
void stc_timer2_isr(void) __interrupt (12);
void stc_i2c_isr(void) __interrupt (24);
#if STC_CORE_UART_AVAILABLE_MASK & 2
void stc_uart2_isr(void) __interrupt (8);
#endif
void stc_uart3_isr(void) __interrupt (17);
void stc_uart4_isr(void) __interrupt (18);
#if STC_CORE_UART_COUNT > 4
void stc_uart5_isr(void) __interrupt (102);
void stc_uart6_isr(void) __interrupt (103);
void stc_uart7_isr(void) __interrupt (104);
void stc_uart8_isr(void) __interrupt (105);
#endif
#if STC_CORE_I2C_COUNT > 1
void stc_i2c2_isr(void) __interrupt (109);
#endif

#if STC_CORE_HAS_INT1
void stc_external1_isr(void) __interrupt (2);
#endif

#if STC_CORE_HAS_UART1 && STC_CORE_SERIAL_BUFFERED_RX
void stc_uart1_isr(void) __interrupt (4);
#endif

int main(void)
{
    /*
     * SDCC startup has already initialized .data/.bss before entering main.
     * Initialize the board-sized XDATA heap before constructors because a
     * global C++ object is allowed to allocate.  The bridge then runs all
     * global C++ constructors before any Arduino lifecycle hook is observable.
     */
    __stcxx_heap_init();
    __stcxx_run_global_ctors();
    init();
    initVariant();
#if ARDUINO_USB_CDC_ON_BOOT
    /* Defer attachment until the first USB poll so setup can register HID. */
    stc_cdc_boot();
#endif
    setup();

    for (;;) {
        loop();
        yield();
    }
}
