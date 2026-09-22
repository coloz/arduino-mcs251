/* Receive entry points link independently of UART setup and transmit.
 * The ISR and polling lookahead retain one shared state provider. */
#include "Arduino.h"
#include "HardwareSerial_private.h"
#include "stc_sfr.h"

int Serial_available(void)
{
#if STC_CORE_HAS_UART1
    if (stc_uart1_started == 0u) {
        return 0;
    }
# if STC_CORE_SERIAL_BUFFERED_RX
    {
        uint8_t head = stc_uart1_rx_head;
        uint8_t tail = stc_uart1_rx_tail;

        if (head >= tail) {
            return (int)(head - tail);
        }
        return (int)((uint8_t)SERIAL_RX_BUFFER_SIZE - tail + head);
    }
#else
    if (stc_uart1_peek_valid != 0u) {
        return 1;
    }
    if ((SCON & STC_SCON_RI) != 0u) {
        stc_uart1_peek_value = SBUF;
        SCON &= (uint8_t)~STC_SCON_RI;
        stc_uart1_peek_valid = 1u;
        return 1;
    }
    return 0;
# endif
#else
    return 0;
#endif
}

int Serial_peek(void)
{
#if STC_CORE_HAS_UART1
# if STC_CORE_SERIAL_BUFFERED_RX
    if ((stc_uart1_started == 0u) || (stc_uart1_rx_head == stc_uart1_rx_tail)) {
        return -1;
    }
    return (int)stc_uart1_rx_buffer[stc_uart1_rx_tail];
#else
    if (Serial_available() == 0) {
        return -1;
    }
    return (int)stc_uart1_peek_value;
# endif
#else
    return -1;
#endif
}

int Serial_read(void)
{
#if STC_CORE_HAS_UART1
# if STC_CORE_SERIAL_BUFFERED_RX
    uint8_t tail;
    uint8_t value;

    if ((stc_uart1_started == 0u) || (stc_uart1_rx_head == stc_uart1_rx_tail)) {
        return -1;
    }
    tail = stc_uart1_rx_tail;
    value = stc_uart1_rx_buffer[tail];
    ++tail;
    if (tail >= (uint8_t)SERIAL_RX_BUFFER_SIZE) {
        tail = 0u;
    }
    stc_uart1_rx_tail = tail;
    return (int)value;
#else
    if (stc_uart1_started == 0u) {
        return -1;
    }
    if (stc_uart1_peek_valid != 0u) {
        stc_uart1_peek_valid = 0u;
        return (int)stc_uart1_peek_value;
    }
    if ((SCON & STC_SCON_RI) != 0u) {
        uint8_t value = SBUF;

        SCON &= (uint8_t)~STC_SCON_RI;
        return (int)value;
    }
    return -1;
# endif
#else
    return -1;
#endif
}

bool Serial_overflow(void)
{
#if STC_CORE_HAS_UART1 && STC_CORE_SERIAL_BUFFERED_RX
    uint8_t saved_ea = (uint8_t)(IE & STC_IE_EA);
    uint8_t result;

    IE &= (uint8_t)~STC_IE_EA;
    result = stc_uart1_rx_overflow;
    stc_uart1_rx_overflow = 0u;
    if (saved_ea != 0u) {
        IE |= STC_IE_EA;
    }
    return (result != 0u);
#else
    return false;
#endif
}
