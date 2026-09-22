/* Small vector shims keep unused UART backends out of sketches without Serial. */
#include "stc_peripheral_irqs.h"
#include "stc_isr_context.h"
STC_IRQ_DATA stc_uart_service_t stc_uart_extra_service;
#define UART_ISR(port, vector) \
    void stc_uart##port##_isr(void) __interrupt (vector) { \
        STC_ISR_CONTEXT_ENTER(); \
        if (stc_uart_extra_service) stc_uart_extra_service(port); \
        STC_ISR_CONTEXT_LEAVE(); \
    }
#if STC_CORE_UART_AVAILABLE_MASK & 2
UART_ISR(2, 8)
#endif
UART_ISR(3, 17)
UART_ISR(4, 18)
#if STC_CORE_UART_COUNT > 4
UART_ISR(5, 102)
UART_ISR(6, 103)
UART_ISR(7, 104)
UART_ISR(8, 105)
#endif
