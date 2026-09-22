#include "Arduino.h"
#include "HardwareSerial_private.h"
#include "stc_isr_context.h"
#include "stc_sfr.h"

#if STC_CORE_HAS_UART1
volatile uint8_t stc_uart1_started;

# if STC_CORE_SERIAL_BUFFERED_RX
#  if defined(STC_XDATA_BYTES) && (STC_XDATA_BYTES > 0)
__xdata uint8_t stc_uart1_rx_buffer[SERIAL_RX_BUFFER_SIZE];
#  else
uint8_t stc_uart1_rx_buffer[SERIAL_RX_BUFFER_SIZE];
#  endif
volatile uint8_t stc_uart1_rx_head;
volatile uint8_t stc_uart1_rx_tail;
volatile uint8_t stc_uart1_rx_overflow;
volatile uint8_t stc_uart1_tx_complete = 1u;

void stc_uart1_isr(void) __interrupt (4)
{
    uint8_t status;

    STC_ISR_CONTEXT_ENTER();
    status = SCON;

    if ((status & STC_SCON_RI) != 0u) {
        uint8_t value = SBUF;
        uint8_t next;

        SCON &= (uint8_t)~STC_SCON_RI;
        if (stc_uart1_started != 0u) {
            next = (uint8_t)(stc_uart1_rx_head + 1u);
            if (next >= (uint8_t)SERIAL_RX_BUFFER_SIZE) {
                next = 0u;
            }
            if (next == stc_uart1_rx_tail) {
                stc_uart1_rx_overflow = 1u;
            } else {
                stc_uart1_rx_buffer[stc_uart1_rx_head] = value;
                stc_uart1_rx_head = next;
            }
        }
    }

    if ((status & STC_SCON_TI) != 0u) {
        SCON &= (uint8_t)~STC_SCON_TI;
        stc_uart1_tx_complete = 1u;
    }
    STC_ISR_CONTEXT_LEAVE();
}
# else
/* Shared even when no buffered UART interrupt is compiled. */
uint8_t stc_uart1_peek_valid;
uint8_t stc_uart1_peek_value;
# endif
#endif
