/* UART2..8, 8N1, independent interrupt RX rings and synchronous TX.
 * STC32G ch. 18; G144 ch. 17/19. Each port owns its numbered timer.
 * All register helpers shared with the ISR are reentrant. */
#include "Arduino.h"
#include "HardwareSerial_ports.h"
#include "HardwareSerial_private.h"
#include "stc_peripheral_irqs.h"
#ifndef STC_SERIAL_HOST_TEST
#include "stc_sfr.h"
__sfr __at (0x9a) S2CON;
__sfr __at (0x9b) S2BUF;
__sfr __at (0xac) S3CON;
__sfr __at (0xad) S3BUF;
__sfr __at (0xfd) S4CON;
__sfr __at (0xfe) S4BUF;
__sfr __at (0xdd) T4T3M;
__sfr __at (0xd4) T3H;
__sfr __at (0xd5) T3L;
__sfr __at (0xd2) T4H;
__sfr __at (0xd3) T4L;
#endif

/* Byte-indexed arrays avoid costly generic structure-pointer arithmetic on MCS251. */
#define EXTRA_UARTS (STC_CORE_UART_COUNT - 1)
#ifndef STC_SERIAL_STATE_IN_DATA
#define STC_SERIAL_STATE_IN_DATA (STC_FLASH_BYTES <= 16384UL)
#endif
#if STC_SERIAL_STATE_IN_DATA
#define UART_STATE STC_IRQ_DATA
#else
#define UART_STATE
#endif
static STC_IRQ_DATA volatile uint8_t uart_active[EXTRA_UARTS], uart_head[EXTRA_UARTS], uart_tail[EXTRA_UARTS];
static STC_IRQ_DATA volatile uint8_t uart_overflow[EXTRA_UARTS], uart_tx_done[EXTRA_UARTS];
static UART_STATE uint8_t uart_custom[EXTRA_UARTS], uart_rx[EXTRA_UARTS], uart_tx[EXTRA_UARTS], uart_route[EXTRA_UARTS];
static UART_STATE uint8_t uart_timer_mode[EXTRA_UARTS], uart_saved_high[EXTRA_UARTS], uart_saved_low[EXTRA_UARTS];
static UART_STATE uint8_t uart_prescaler[EXTRA_UARTS], uart_clock_output[EXTRA_UARTS];
static UART_STATE uint8_t uart_mux[EXTRA_UARTS];
#if STC_CORE_UART_COUNT > 4
static UART_STATE uint8_t uart_mux_high[EXTRA_UARTS];
#endif
static uint8_t uart_buffer[EXTRA_UARTS][SERIAL_RX_BUFFER_SIZE];

static uint8_t valid(uint8_t port)
{
    return port >= 2u && port <= STC_CORE_UART_COUNT &&
           (STC_CORE_UART_AVAILABLE_MASK & (1u << (port - 1u)));
}
static uint8_t route_for(uint8_t port, uint8_t rx, uint8_t tx)
{
    switch (port) {
    case 2: return STC_VARIANT_UART2_ROUTE(rx, tx);
    case 3: return STC_VARIANT_UART3_ROUTE(rx, tx);
    case 4: return STC_VARIANT_UART4_ROUTE(rx, tx);
#if STC_CORE_UART_COUNT > 4
    case 5: return STC_VARIANT_UART5_ROUTE(rx, tx);
    case 6: return STC_VARIANT_UART6_ROUTE(rx, tx);
    case 7: return STC_VARIANT_UART7_ROUTE(rx, tx);
    case 8: return STC_VARIANT_UART8_ROUTE(rx, tx);
#endif
    }
    return 255u;
}
static void default_pins(uint8_t port, uint8_t index)
{
    switch (port) {
    case 2: uart_rx[index] = PIN_SERIAL2_RX; uart_tx[index] = PIN_SERIAL2_TX; break;
    case 3: uart_rx[index] = PIN_SERIAL3_RX; uart_tx[index] = PIN_SERIAL3_TX; break;
    case 4: uart_rx[index] = PIN_SERIAL4_RX; uart_tx[index] = PIN_SERIAL4_TX; break;
#if STC_CORE_UART_COUNT > 4
    case 5: uart_rx[index] = PIN_SERIAL5_RX; uart_tx[index] = PIN_SERIAL5_TX; break;
    case 6: uart_rx[index] = PIN_SERIAL6_RX; uart_tx[index] = PIN_SERIAL6_TX; break;
    case 7: uart_rx[index] = PIN_SERIAL7_RX; uart_tx[index] = PIN_SERIAL7_TX; break;
    case 8: uart_rx[index] = PIN_SERIAL8_RX; uart_tx[index] = PIN_SERIAL8_TX; break;
#endif
    }
    uart_route[index] = route_for(port, uart_rx[index], uart_tx[index]);
}

/* EAXFR is held only for short register operations, never across TX waits. */
static uint8_t enter(void) STC_REENTRANT
{
    uint8_t saved = P_SW2 & 0x80u;
    P_SW2 |= 0x80u;
    return saved;
}
static void leave(uint8_t saved) STC_REENTRANT
{
    P_SW2 = (P_SW2 & 0x7fu) | saved;
}
#if STC_CORE_UART_COUNT > 4
static unsigned long uart_base(uint8_t port) STC_REENTRANT
{
    return 0x7ef740UL + (unsigned long)(port - 5u) * 0x30UL;
}
static unsigned long timer_base(uint8_t port)
{
    return port < 7u ? 0x7efbb0UL : 0x7efbb8UL;
}
#endif
static uint8_t control_read(uint8_t port) STC_REENTRANT
{
    if (port == 2u) return S2CON;
    if (port == 3u) return S3CON;
    if (port == 4u) return S4CON;
#if STC_CORE_UART_COUNT > 4
    return STC_XFR8(uart_base(port));
#else
    return 0u;
#endif
}
static void control_write(uint8_t port, uint8_t value) STC_REENTRANT
{
    if (port == 2u) S2CON = value;
    else if (port == 3u) S3CON = value;
    else if (port == 4u) S4CON = value;
#if STC_CORE_UART_COUNT > 4
    else STC_XFR8(uart_base(port)) = value;
#endif
}
static uint8_t data_read(uint8_t port) STC_REENTRANT
{
    if (port == 2u) return S2BUF;
    if (port == 3u) return S3BUF;
    if (port == 4u) return S4BUF;
#if STC_CORE_UART_COUNT > 4
    return STC_XFR8(uart_base(port) + 1UL);
#else
    return 0u;
#endif
}
static void data_write(uint8_t port, uint8_t value)
{
    if (port == 2u) S2BUF = value;
    else if (port == 3u) S3BUF = value;
    else if (port == 4u) S4BUF = value;
#if STC_CORE_UART_COUNT > 4
    else STC_XFR8(uart_base(port) + 1UL) = value;
#endif
}
static uint8_t irq_mask(uint8_t port)
{
    return port == 2u ? 1u : port == 3u ? 8u : 16u;
}
static void irq_enable(uint8_t port, uint8_t enabled)
{
    uint8_t mask = irq_mask(port);
    if (port <= 4u) IE2 = (IE2 & (uint8_t)~mask) | (enabled ? mask : 0u);
#if STC_CORE_UART_COUNT > 4
    else {
        unsigned long addr = uart_base(port) + 2UL;
        STC_XFR8(addr) = (STC_XFR8(addr) & 0x7fu) | (enabled ? 0x80u : 0u);
    }
#endif
}
static void receive_interrupt(uint8_t port) STC_REENTRANT
{
    uint8_t index = port - 2u;
    uint8_t saved = enter(), status = control_read(port), next, value;
    if (status & 1u) {
        value = data_read(port);
        control_write(port, control_read(port) & (uint8_t)~1u);
        if (uart_active[index]) {
            next = uart_head[index] + 1u;
            if (next >= SERIAL_RX_BUFFER_SIZE) next = 0u;
            if (next == uart_tail[index]) uart_overflow[index] = 1u;
            else { uart_buffer[index][uart_head[index]] = value; uart_head[index] = next; }
        }
    }
    if (status & 2u) {
        control_write(port, control_read(port) & (uint8_t)~2u);
        uart_tx_done[index] = 1u;
    }
    leave(saved);
}

/* Snapshot only the selected timer's fields. Sibling timers and routes stay live. */
static uint8_t timer_busy(uint8_t port)
{
    if (port == 2u) return (AUXR & 0x10u) || (IE2 & 4u) ||
        ((AUXR & 1u) && (SCON & 0x10u));
    if (port == 3u) return (T4T3M & 8u) || (IE2 & 0x20u);
    if (port == 4u) return (T4T3M & 0x80u) || (IE2 & 0x40u);
#if STC_CORE_UART_COUNT > 4
    return (STC_XFR8(timer_base(port) + 3UL) & ((port & 1u) ? 8u : 0x80u)) ||
           (STC_XFR8(timer_base(port) + 2UL) & ((port & 1u) ? 1u : 0x10u));
#else
    return 1u;
#endif
}
static void timer_start(uint8_t port, uint8_t index, uint16_t reload)
{
    uint8_t mask = port == 3u || (port & 1u) ? 0x0fu : 0xf0u;
    if (port <= 4u) {
        uart_prescaler[index] = STC_XFR8(0x7efea0UL + port);
        STC_XFR8(0x7efea0UL + port) = 0u;
        if (port == 2u) {
            uart_timer_mode[index] = AUXR & 0x1cu; uart_saved_high[index] = T2H; uart_saved_low[index] = T2L;
            uart_clock_output[index] = INTCLKO & 4u; INTCLKO &= (uint8_t)~4u;
            AUXR = (AUXR & (uint8_t)~0x1cu) | 4u;
            T2H = reload >> 8; T2L = (uint8_t)reload; AUXR |= 0x10u;
        } else {
            uart_timer_mode[index] = T4T3M & mask; T4T3M &= (uint8_t)~mask;
            if (port == 3u) { uart_saved_high[index] = T3H; uart_saved_low[index] = T3L; T3H = reload >> 8; T3L = (uint8_t)reload; }
            else { uart_saved_high[index] = T4H; uart_saved_low[index] = T4L; T4H = reload >> 8; T4L = (uint8_t)reload; }
            T4T3M |= mask & 0xaau;
        }
    }
#if STC_CORE_UART_COUNT > 4
    else {
        unsigned long base = timer_base(port), hi = base + ((port & 1u) ? 6UL : 4UL);
        unsigned long ps = base + ((port & 1u) ? 0UL : 1UL);
        uart_prescaler[index] = STC_XFR8(ps); STC_XFR8(ps) = 0u;
        uart_timer_mode[index] = STC_XFR8(base + 3UL) & mask;
        STC_XFR8(base + 3UL) &= (uint8_t)~mask;
        uart_saved_high[index] = STC_XFR8(hi); uart_saved_low[index] = STC_XFR8(hi + 1UL);
        STC_XFR8(hi) = reload >> 8; STC_XFR8(hi + 1UL) = (uint8_t)reload;
        STC_XFR8(base + 3UL) |= mask & 0xaau;
    }
#endif
}
static void timer_stop(uint8_t port, uint8_t index)
{
    uint8_t mask = port == 3u || (port & 1u) ? 0x0fu : 0xf0u;
    if (port <= 4u) {
        if (port == 2u) {
            AUXR &= (uint8_t)~0x1cu;
            T2H = uart_saved_high[index]; T2L = uart_saved_low[index];
            INTCLKO = (INTCLKO & (uint8_t)~4u) | uart_clock_output[index];
            AUXR |= uart_timer_mode[index];
        } else {
            T4T3M &= (uint8_t)~mask;
            if (port == 3u) { T3H = uart_saved_high[index]; T3L = uart_saved_low[index]; }
            else { T4H = uart_saved_high[index]; T4L = uart_saved_low[index]; }
            T4T3M |= uart_timer_mode[index];
        }
        STC_XFR8(0x7efea0UL + port) = uart_prescaler[index];
    }
#if STC_CORE_UART_COUNT > 4
    else {
        unsigned long base = timer_base(port), hi = base + ((port & 1u) ? 6UL : 4UL);
        STC_XFR8(base + 3UL) &= (uint8_t)~mask;
        STC_XFR8(hi) = uart_saved_high[index]; STC_XFR8(hi + 1UL) = uart_saved_low[index];
        STC_XFR8(base + ((port & 1u) ? 0UL : 1UL)) = uart_prescaler[index];
        STC_XFR8(base + 3UL) |= uart_timer_mode[index];
    }
#endif
}
static void mux_configure(uint8_t port, uint8_t index, uint8_t restore)
{
    uint8_t mask, value;
    if (port <= 4u) {
        mask = 1u << (port - 2u);
        value = P_SW2 & mask;
        P_SW2 = (P_SW2 & (uint8_t)~mask) | (restore ? uart_mux[index] : ((uart_route[index] & 1u) ? mask : 0u));
        if (!restore) uart_mux[index] = value;
#if STC_CORE_UART_COUNT > 4
        mask = 1u << (port + 3u);
        value = STC_XFR8(0x7efd69UL) & mask;
        STC_XFR8(0x7efd69UL) = (STC_XFR8(0x7efd69UL) & (uint8_t)~mask) |
            (restore ? uart_mux_high[index] : ((uart_route[index] & 2u) ? mask : 0u));
        if (!restore) uart_mux_high[index] = value;
#endif
    }
#if STC_CORE_UART_COUNT > 4
    else {
        unsigned long addr = port < 7u ? 0x7efd6aUL : 0x7efd6dUL;
        uint8_t shift = port == 5u ? 0u : port == 6u ? 2u : port == 7u ? 4u : 6u;
        mask = 3u << shift; value = STC_XFR8(addr) & mask;
        STC_XFR8(addr) = (STC_XFR8(addr) & (uint8_t)~mask) | (restore ? uart_mux[index] : uart_route[index] << shift);
        if (!restore) uart_mux[index] = value;
    }
#endif
}
bool SerialPort_setPinsChecked(uint8_t port, uint8_t rx, uint8_t tx)
{
    uint8_t index;
    uint8_t route;
    if (!valid(port)) return false;
    index = port - 2u; route = route_for(port, rx, tx);
    if (uart_active[index] || route == 255u) return false;
    uart_rx[index] = rx; uart_tx[index] = tx; uart_route[index] = route; uart_custom[index] = 1u;
    return true;
}
bool SerialPort_begin(uint8_t port, unsigned long baud)
{
    uint8_t index;
    unsigned long divisor, denominator, ticks;
    uint8_t enabled, saved;
    if (!valid(port) || !baud || baud > F_CPU / 4UL) return false;
    denominator = baud * 4UL;
    divisor = (F_CPU + denominator / 2UL) / denominator;
    if (!divisor || divisor > 65535UL) return false;
    ticks = divisor * denominator;
    /* |F_CPU - ticks| / ticks <= 3%. Constant bounds avoid another
     * 32-bit divide/modulo, and the split products cannot overflow. */
    if (ticks < (F_CPU / 103UL) * 100UL + ((F_CPU % 103UL) * 100UL + 102UL) / 103UL ||
        ticks > (F_CPU / 97UL) * 100UL + ((F_CPU % 97UL) * 100UL) / 97UL) return false;
    index = port - 2u;
    if (!uart_custom[index]) default_pins(port, index);
    if (uart_route[index] == 255u) return false;
    if (uart_active[index]) SerialPort_end(port);
    enabled = IE & 0x80u; IE &= 0x7fu; saved = enter();
    if (timer_busy(port) || (control_read(port) & 0x10u)) {
        leave(saved); IE |= enabled; return false;
    }
    irq_enable(port, 0u);
    mux_configure(port, index, 0u);
    digitalWrite(uart_tx[index], HIGH); pinMode(uart_tx[index], OUTPUT); pinMode(uart_rx[index], INPUT_PULLUP);
    uart_head[index] = uart_tail[index] = uart_overflow[index] = 0u; uart_tx_done[index] = 1u;
#if STC_CORE_UART_COUNT > 4
    {
        unsigned long controls = port <= 4u ? 0x7efdc8UL + (port - 2u) * 8UL : uart_base(port) + 8UL;
        /* Disable synchronous, LIN, IrDA, half-duplex and parity modes. */
        STC_XFR8(controls) = 0u; STC_XFR8(controls + 1UL) = 0u;
        if (port <= 4u) STC_XFR8(0x7efdb4UL + (port - 2u) * 4UL) = 1u;
        else STC_XFR8(uart_base(port) + 0x2dUL) = 1u;
    }
#endif
    /* UART2 uses USART mode 1; classic UART3/4 use bit 6 to select T3/T4.
     * G144 UART3..8 use mode 1 and their separate SnCFG.SnBRT bit. */
    control_write(port, 0x50u);
    stc_uart_extra_service = receive_interrupt;
    uart_active[index] = 1u;
    timer_start(port, index, (uint16_t)(65536UL - divisor));
    irq_enable(port, 1u);
    leave(saved); IE |= enabled;
    return true;
}
void SerialPort_flush(uint8_t port)
{
    uint8_t index;
    uint8_t saved, enabled;
    if (!valid(port)) return;
    index = port - 2u;
    while (uart_active[index] && !uart_tx_done[index]) {
        enabled = IE & 0x80u; IE &= 0x7fu; saved = enter();
        if (control_read(port) & 2u) { control_write(port, control_read(port) & (uint8_t)~2u); uart_tx_done[index] = 1u; }
        leave(saved); IE |= enabled;
    }
}
void SerialPort_end(uint8_t port)
{
    uint8_t index;
    uint8_t enabled, saved;
    if (!valid(port)) return;
    index = port - 2u; if (!uart_active[index]) return;
    SerialPort_flush(port);
    enabled = IE & 0x80u; IE &= 0x7fu; saved = enter();
    irq_enable(port, 0u); control_write(port, 0u); uart_active[index] = 0u;
    timer_stop(port, index); mux_configure(port, index, 1u);
    uart_head[index] = uart_tail[index] = uart_overflow[index] = 0u;
    pinMode(uart_rx[index], INPUT); pinMode(uart_tx[index], INPUT);
    leave(saved); IE |= enabled;
}
int SerialPort_available(uint8_t port)
{
    uint8_t index; uint8_t head, tail;
    if (!valid(port)) return 0;
    index = port - 2u; if (!uart_active[index]) return 0;
    head = uart_head[index]; tail = uart_tail[index];
    return head >= tail ? head - tail : SERIAL_RX_BUFFER_SIZE - tail + head;
}
int SerialPort_peek(uint8_t port)
{
    uint8_t index;
    if (!SerialPort_available(port)) return -1;
    index = port - 2u; return uart_buffer[index][uart_tail[index]];
}
int SerialPort_read(uint8_t port)
{
    uint8_t index; uint8_t tail; int value = SerialPort_peek(port);
    if (value < 0) return -1;
    index = port - 2u; tail = uart_tail[index] + 1u;
    if (tail >= SERIAL_RX_BUFFER_SIZE) tail = 0u;
    uart_tail[index] = tail; return value;
}
int SerialPort_availableForWrite(uint8_t port)
{
    return valid(port) && uart_active[port - 2u] && uart_tx_done[port - 2u] ? 1 : 0;
}
size_t SerialPort_write(uint8_t port, uint8_t value)
{
    uint8_t enabled, saved;
    if (!valid(port) || !uart_active[port - 2u]) return 0u;
    SerialPort_flush(port);
    enabled = IE & 0x80u; IE &= 0x7fu; saved = enter();
    uart_tx_done[port - 2u] = 0u;
    control_write(port, control_read(port) & (uint8_t)~2u); data_write(port, value);
#ifdef STC_SERIAL_HOST_TEST
    stc_serial_test_transmit(port, value);
#endif
    leave(saved); IE |= enabled;
    SerialPort_flush(port); return 1u;
}
bool SerialPort_overflow(uint8_t port)
{
    uint8_t enabled, value;
    if (!valid(port)) return false;
    enabled = IE & 0x80u; IE &= 0x7fu;
    value = uart_overflow[port - 2u]; uart_overflow[port - 2u] = 0u;
    IE |= enabled; return value != 0u;
}
