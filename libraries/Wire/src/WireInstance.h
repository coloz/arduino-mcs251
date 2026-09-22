/*
 * SPDX-License-Identifier: MIT
 *
 * Software fallback for targets without a common hardware I2C peripheral.
 * Timing is approximate because it uses the core delayMicroseconds() API.
 */
#include "Wire_backend.h"
#include "stc_peripheral_irqs.h"
#if defined(__SDCC)
# include "stc_sfr.h"
#endif

#define WIRE_INTERNAL_ACK      0u
#define WIRE_INTERNAL_NACK     1u
#define WIRE_INTERNAL_TIMEOUT  2u

static STC_WIRE_HOT uint8_t wire_sda_pin = WIRE_DEFAULT_SDA_PIN;
static STC_WIRE_HOT uint8_t wire_scl_pin = WIRE_DEFAULT_SCL_PIN;
static uint8_t wire_pins_custom;
static STC_WIRE_HOT unsigned int wire_half_period_us;
static unsigned long wire_stretch_timeout_us =
    WIRE_DEFAULT_STRETCH_TIMEOUT_US;
static uint8_t wire_reset_with_timeout;
static uint8_t wire_timeout_flag;
static STC_WIRE_HOT uint8_t wire_initialized;
static STC_WIRE_HOT uint8_t wire_bus_held;
static uint8_t wire_config_error;
static uint8_t wire_last_error;

#if defined(__SDCC_mcs251)
/* The default pair is owned/configured as open drain by Wire_begin(). Bit
 * instructions avoid pin validation, PWM detachment and port dispatch on
 * every bus edge, and cannot overwrite unrelated P3 latch bits from an ISR.
 * Other pin selections retain the portable GPIO path. */
static __sbit __at (0xb2) wire_p32;
static __sbit __at (0xb3) wire_p33;
static uint8_t wire_fast_default;
#endif

uint8_t Wire_configurationError(void) STC_WIRE_REENTRANT { return wire_config_error; }
uint8_t Wire_lastError(void) STC_WIRE_REENTRANT { return wire_last_error; }

static uint8_t wire_tx_address;
static uint8_t wire_tx_buffer[WIRE_BUFFER_LENGTH];
static STC_WIRE_HOT uint8_t wire_tx_length;
static uint8_t wire_tx_overflow;
static uint8_t wire_transmitting;

static uint8_t wire_rx_buffer[WIRE_BUFFER_LENGTH];
static STC_WIRE_HOT uint8_t wire_rx_index;
static uint8_t wire_rx_length;
static uint8_t wire_slave, wire_slave_address, wire_slave_receiving;
static uint8_t wire_slave_expect_address, wire_slave_tx_index;
static void (*wire_receive_callback)(int) STC_WIRE_REENTRANT;
static void (*wire_request_callback)(void) STC_WIRE_REENTRANT;

#include "WireHardware.h"
static void wire_release_bus(void);

static uint8_t wire_irq_lock(void)
{
#if defined(__SDCC)
    uint8_t enabled = IE & STC_IE_EA;
    IE &= (uint8_t)~STC_IE_EA;
    return enabled;
#else
    return 0u;
#endif
}
static void wire_irq_unlock(uint8_t enabled)
{
#if defined(__SDCC)
    IE |= enabled;
#else
    (void)enabled;
#endif
}
static void wire_slave_received(void) STC_WIRE_REENTRANT
{
    if (wire_slave_receiving) {
        wire_slave_receiving = 0u;
        wire_rx_index = 0u;
        if (wire_receive_callback) wire_receive_callback((int)wire_rx_length);
    }
}
static void wire_slave_interrupt(void) STC_WIRE_REENTRANT
{
#ifdef STC_WIRE_HARDWARE
    uint8_t saved = wire_hw_enter(), status = WIRE_HW_READ(4u), value;
    if (status & 0x40u) {
        /* Deliver write data before onRequest on a repeated START. */
        wire_slave_received();
        wire_slave_expect_address = 1u;
        WIRE_HW_WRITE(4u, WIRE_HW_READ(4u) & (uint8_t)~0x41u);
    }
    if (status & 0x20u) {
        value = WIRE_HW_READ(7u);
        if (wire_slave_expect_address) {
            wire_slave_expect_address = 0u;
            wire_slave_tx_index = 0u;
            if (value & 1u) {
                wire_tx_length = 0u; wire_tx_overflow = 0u;
                wire_transmitting = 1u;
                if (wire_request_callback) wire_request_callback();
                wire_transmitting = 0u;
                WIRE_HW_WRITE(6u, wire_tx_length ? wire_tx_buffer[wire_slave_tx_index++] : 0xffu);
            } else {
                wire_rx_length = 0u; wire_rx_index = 0u;
                wire_slave_receiving = 1u;
            }
            WIRE_HW_WRITE(4u, WIRE_HW_READ(4u) & (uint8_t)~0x21u);
        } else {
            if (wire_rx_length < WIRE_BUFFER_LENGTH) {
                wire_rx_buffer[wire_rx_length++] = value;
                WIRE_HW_WRITE(4u, WIRE_HW_READ(4u) & (uint8_t)~0x21u);
            } else {
                wire_last_error = WIRE_STATUS_DATA_TOO_LONG;
                WIRE_HW_WRITE(4u, (WIRE_HW_READ(4u) & (uint8_t)~0x20u) | 1u);
            }
        }
    }
    if (status & 0x10u) {
        value = 0xffu;
        if (!(status & 2u) && wire_slave_tx_index < wire_tx_length)
            value = wire_tx_buffer[wire_slave_tx_index++];
        WIRE_HW_WRITE(6u, value);
        WIRE_HW_WRITE(4u, WIRE_HW_READ(4u) & (uint8_t)~0x10u);
    }
    if (status & 8u) {
        wire_slave_received(); wire_slave_expect_address = 1u;
        WIRE_HW_WRITE(4u, WIRE_HW_READ(4u) & (uint8_t)~9u);
    }
    wire_hw_leave(saved);
#endif
}
void Wire_onReceive(void (*callback)(int) STC_WIRE_REENTRANT) STC_WIRE_REENTRANT
{
    uint8_t enabled = wire_irq_lock();
    wire_receive_callback = callback;
    wire_irq_unlock(enabled);
}
void Wire_onRequest(void (*callback)(void) STC_WIRE_REENTRANT) STC_WIRE_REENTRANT
{
    uint8_t enabled = wire_irq_lock();
    wire_request_callback = callback;
    wire_irq_unlock(enabled);
}
void Wire_beginSlave(uint8_t address) STC_WIRE_REENTRANT
{
#ifdef STC_WIRE_HARDWARE
    uint8_t saved, enabled, route;
    if (!wire_pins_custom && !wire_initialized) {
        wire_sda_pin = WIRE_HW_DEFAULT_SDA; wire_scl_pin = WIRE_HW_DEFAULT_SCL;
    }
    route = WIRE_HW_ROUTE(wire_sda_pin, wire_scl_pin);
    if (!address || address > 0x7fu || route == 255u) {
        wire_config_error = WIRE_STATUS_OTHER_ERROR; return;
    }
    Wire_end();
    enabled = wire_irq_lock();
    pinMode(wire_sda_pin, OUTPUT_OPEN_DRAIN);
    pinMode(wire_scl_pin, OUTPUT_OPEN_DRAIN);
    wire_release_bus();
    saved = wire_hw_enter();
    wire_saved_mux = WIRE_HW_MUX_READ() & WIRE_HW_MUX_MASK;
    WIRE_HW_MUX_WRITE((WIRE_HW_MUX_READ() & (uint8_t)~WIRE_HW_MUX_MASK) |
                      (uint8_t)(route << WIRE_HW_MUX_SHIFT));
    WIRE_HW_WRITE(0u, 0u); WIRE_HW_WRITE(8u, 0u);
    WIRE_HW_WRITE(3u, 1u); /* Reset the slave before installing callbacks. */
    WIRE_HW_WRITE(4u, 0u); WIRE_HW_WRITE(5u, (uint8_t)(address << 1));
    wire_slave = 1u; wire_slave_address = address;
    wire_slave_expect_address = 1u; wire_slave_receiving = 0u;
    wire_rx_index = wire_rx_length = wire_tx_length = 0u;
    wire_transmitting = 0u; wire_hardware = 1u; wire_initialized = 1u;
    stc_wire_slave_service = wire_slave_interrupt;
    WIRE_HW_WRITE(3u, 0x78u); WIRE_HW_WRITE(0u, 0x80u);
    wire_hw_leave(saved);
    wire_config_error = wire_last_error = WIRE_STATUS_SUCCESS;
    wire_irq_unlock(enabled);
#else
    (void)address;
    wire_config_error = WIRE_STATUS_OTHER_ERROR;
#endif
}

static void wire_delay_half_period(void)
{
    delayMicroseconds(wire_half_period_us);
}

static void wire_drive_sda_low(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) { wire_p32 = 0; return; }
#endif
    digitalWrite(wire_sda_pin, LOW);
}

static void wire_release_sda(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) { wire_p32 = 1; return; }
#endif
    digitalWrite(wire_sda_pin, HIGH);
}

static void wire_drive_scl_low(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) { wire_p33 = 0; return; }
#endif
    digitalWrite(wire_scl_pin, LOW);
}

static void wire_release_scl(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) { wire_p33 = 1; return; }
#endif
    digitalWrite(wire_scl_pin, HIGH);
}

static uint8_t wire_read_sda(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) return wire_p32;
#endif
    return digitalRead(wire_sda_pin) != LOW;
}

static uint8_t wire_read_scl(void)
{
#if defined(__SDCC_mcs251)
    if (wire_fast_default) return wire_p33;
#endif
    return digitalRead(wire_scl_pin) != LOW;
}

static void wire_handle_timeout(void)
{
    wire_timeout_flag = 1u;
    if (wire_reset_with_timeout == 0u) {
        return;
    }
#ifdef STC_WIRE_HARDWARE
    if (wire_hardware) wire_hardware_reset();
#endif

    /* Also reset the shared transaction state and release both open-drain
     * latches; the hardware block, when selected, was reset above. */
    wire_release_sda();
    wire_release_scl();
    wire_bus_held = 0u;
    wire_tx_length = 0u;
    wire_tx_overflow = 0u;
    wire_transmitting = 0u;
    wire_rx_index = 0u;
    wire_rx_length = 0u;
}

static uint8_t wire_wait_for_scl_high(void)
{
    unsigned long started;

    wire_release_scl();
    /* Most slaves do not stretch the clock. Avoid the comparatively expensive
     * micros() conversion on every bit when SCL is already high. A low line
     * still follows the original bounded/unbounded stretch path below. */
    if (wire_read_scl()) {
        return 1u;
    }
    if (wire_stretch_timeout_us == 0UL) {
        while (!wire_read_scl()) {
            /* A zero timeout deliberately preserves Arduino's wait-forever
             * behavior. */
        }
        return 1u;
    }

    started = micros();
    while (!wire_read_scl()) {
        if ((unsigned long)(micros() - started) >= wire_stretch_timeout_us) {
            wire_handle_timeout();
            return 0u;
        }
    }
    return 1u;
}

static void wire_release_bus(void)
{
    wire_release_sda();
    wire_release_scl();
    wire_bus_held = 0u;
}

static void wire_ensure_initialized(void)
{
    if (wire_initialized == 0u) {
        Wire_begin();
    }
}

static uint8_t wire_start_condition(void)
{
    if (wire_slave) return WIRE_STATUS_OTHER_ERROR;
#if defined(__SDCC)
    /* The timeout clock needs Timer0 interrupts. Wire is a foreground API. */
    if (!(IE & STC_IE_EA) || !(IE & STC_IE_ET0) || !(TCON & STC_TCON_TR0))
        return WIRE_STATUS_OTHER_ERROR;
#endif
    if (wire_config_error) return WIRE_STATUS_OTHER_ERROR;
#ifdef STC_WIRE_HARDWARE
    if (wire_hardware) {
        if (!wire_bus_held && digitalRead(wire_sda_pin) == LOW) return WIRE_STATUS_OTHER_ERROR;
        if (!wire_hardware_command(1u)) return WIRE_STATUS_TIMEOUT;
        wire_bus_held = 1u; return WIRE_STATUS_SUCCESS;
    }
#endif
    wire_release_sda();
    wire_delay_half_period();

    if (wire_wait_for_scl_high() == 0u) {
        wire_release_bus();
        return WIRE_STATUS_TIMEOUT;
    }
    wire_delay_half_period();

    if (!wire_read_sda()) {
        wire_release_bus();
        return WIRE_STATUS_OTHER_ERROR;
    }

    wire_drive_sda_low();
    wire_delay_half_period();
    wire_drive_scl_low();
    wire_delay_half_period();
    wire_bus_held = 1u;
    return WIRE_STATUS_SUCCESS;
}

static uint8_t wire_stop_condition(void)
{
    uint8_t status = WIRE_STATUS_SUCCESS;
#ifdef STC_WIRE_HARDWARE
    if (wire_hardware) {
        status = wire_hardware_command(6u) ? WIRE_STATUS_SUCCESS : WIRE_STATUS_TIMEOUT;
        wire_bus_held = 0u; return status;
    }
#endif

    wire_drive_sda_low();
    wire_delay_half_period();
    if (wire_wait_for_scl_high() == 0u) {
        status = WIRE_STATUS_TIMEOUT;
    } else {
        wire_delay_half_period();
    }
    wire_release_sda();
    wire_delay_half_period();
    wire_bus_held = 0u;
    return status;
}

static uint8_t wire_write_byte(uint8_t value)
{
    uint8_t mask;
    uint8_t acknowledged;
#ifdef STC_WIRE_HARDWARE
    if (wire_hardware) return wire_hardware_write(value);
#endif

    for (mask = 0x80u; mask != 0u; mask >>= 1) {
        wire_drive_scl_low();
        if ((value & mask) != 0u) {
            wire_release_sda();
        } else {
            wire_drive_sda_low();
        }
        wire_delay_half_period();
        if (wire_wait_for_scl_high() == 0u) {
            wire_release_sda();
            return WIRE_INTERNAL_TIMEOUT;
        }
        wire_delay_half_period();
        wire_drive_scl_low();
    }

    wire_release_sda();
    wire_delay_half_period();
    if (wire_wait_for_scl_high() == 0u) {
        wire_drive_scl_low();
        return WIRE_INTERNAL_TIMEOUT;
    }
    wire_delay_half_period();
    acknowledged = !wire_read_sda();
    wire_drive_scl_low();
    wire_delay_half_period();

    return (acknowledged != 0u) ? WIRE_INTERNAL_ACK : WIRE_INTERNAL_NACK;
}

static uint8_t wire_read_byte(uint8_t send_ack, uint8_t *value)
{
    uint8_t mask;
    uint8_t result = 0u;
#ifdef STC_WIRE_HARDWARE
    if (wire_hardware) return wire_hardware_read(send_ack, value);
#endif

    wire_release_sda();
    for (mask = 0x80u; mask != 0u; mask >>= 1) {
        wire_drive_scl_low();
        wire_delay_half_period();
        if (wire_wait_for_scl_high() == 0u) {
            wire_drive_scl_low();
            return 0u;
        }
        wire_delay_half_period();
        if (wire_read_sda()) {
            result |= mask;
        }
        wire_drive_scl_low();
    }

    if (send_ack != 0u) {
        wire_drive_sda_low();
    } else {
        wire_release_sda();
    }
    wire_delay_half_period();
    if (wire_wait_for_scl_high() == 0u) {
        wire_drive_scl_low();
        wire_release_sda();
        return 0u;
    }
    wire_delay_half_period();
    wire_drive_scl_low();
    wire_release_sda();
    wire_delay_half_period();

    *value = result;
    return 1u;
}

void Wire_begin(void) STC_WIRE_REENTRANT
{
    if (wire_slave) Wire_end();
    if (!digitalPinIsValid(wire_sda_pin) || !digitalPinIsValid(wire_scl_pin) ||
        wire_sda_pin == wire_scl_pin || STC_VARIANT_PHYSICAL_ALIAS(wire_sda_pin) == wire_scl_pin) {
        wire_config_error = WIRE_STATUS_OTHER_ERROR;
        return;
    }
    if (wire_half_period_us == 0u) {
        Wire_setClock(WIRE_DEFAULT_CLOCK_HZ);
    }
    pinMode(wire_sda_pin, OUTPUT_OPEN_DRAIN);
    pinMode(wire_scl_pin, OUTPUT_OPEN_DRAIN);
#ifdef STC_WIRE_HARDWARE
    wire_hardware_begin();
#endif
#if defined(__SDCC_mcs251)
    wire_fast_default = wire_sda_pin == P3_2 && wire_scl_pin == P3_3;
#endif
    wire_release_bus();
    wire_tx_length = 0u;
    wire_tx_overflow = 0u;
    wire_transmitting = 0u;
    wire_rx_index = 0u;
    wire_rx_length = 0u;
    wire_initialized = 1u;
}

void Wire_end(void) STC_WIRE_REENTRANT
{
    uint8_t enabled = wire_irq_lock();
    stc_wire_slave_service = 0;
#ifdef STC_WIRE_HARDWARE
    if (wire_slave) wire_hardware_end();
#endif
    wire_slave = 0u;
    wire_slave_receiving = 0u;
    /* A master's STOP can wait for SCL/timeout. Timer0 must keep running;
     * the slave controller and callback have already been disabled. */
    wire_irq_unlock(enabled);
    if (wire_initialized == 0u) {
        return;
    }
    if (wire_bus_held != 0u) {
        (void)wire_stop_condition();
    }
#ifdef STC_WIRE_HARDWARE
    wire_hardware_end();
#endif
    wire_release_bus();
    pinMode(wire_sda_pin, INPUT);
    pinMode(wire_scl_pin, INPUT);
    wire_tx_length = 0u;
    wire_tx_overflow = 0u;
    wire_transmitting = 0u;
    wire_rx_index = 0u;
    wire_rx_length = 0u;
    wire_initialized = 0u;
}

void Wire_setPins(uint8_t sda_pin, uint8_t scl_pin) STC_WIRE_REENTRANT
{
    (void)Wire_setPinsChecked(sda_pin, scl_pin);
}

uint8_t Wire_setPinsChecked(uint8_t sda_pin, uint8_t scl_pin) STC_WIRE_REENTRANT
{
    uint8_t restart = wire_initialized;
    uint8_t slave = wire_slave, address = wire_slave_address;
    if (!digitalPinIsValid(sda_pin) || !digitalPinIsValid(scl_pin) ||
        sda_pin == scl_pin || STC_VARIANT_PHYSICAL_ALIAS(sda_pin) == scl_pin ||
        (slave && WIRE_HW_ROUTE(sda_pin, scl_pin) == 255u)) {
        return wire_config_error = WIRE_STATUS_OTHER_ERROR;
    }
    if (restart != 0u) {
        Wire_end();
    }
    wire_sda_pin = sda_pin;
    wire_scl_pin = scl_pin;
    wire_pins_custom = 1u;
    if (restart != 0u) {
        if (slave) Wire_beginSlave(address); else Wire_begin();
    }
    return wire_config_error = WIRE_STATUS_SUCCESS;
}

void Wire_setClock(unsigned long clock_hz) STC_WIRE_REENTRANT
{
    unsigned long half_period;

    if (clock_hz == 0UL) {
        wire_config_error = WIRE_STATUS_OTHER_ERROR;
        return;
    }
#ifdef STC_WIRE_HARDWARE
    if (wire_bus_held) { wire_config_error = WIRE_STATUS_OTHER_ERROR; return; }
    wire_clock_hz = clock_hz;
#endif
    if (clock_hz >= 500000UL) {
        half_period = 1UL;
    } else {
        half_period = (500000UL + clock_hz - 1UL) / clock_hz;
    }
    if (half_period > 65535UL) {
        half_period = 65535UL;
    }
    wire_half_period_us = (unsigned int)half_period;
#ifdef STC_WIRE_HARDWARE
    if (wire_initialized && !wire_slave) wire_hardware_begin();
#endif
    wire_config_error = WIRE_STATUS_SUCCESS;
}

void Wire_setClockStretchTimeout(unsigned long timeout_us) STC_WIRE_REENTRANT
{
    Wire_setWireTimeout((uint32_t)timeout_us, 0u);
}

void Wire_setWireTimeout(uint32_t timeout_us,
                         uint8_t reset_with_timeout) STC_WIRE_REENTRANT
{
    wire_stretch_timeout_us = (unsigned long)timeout_us;
    wire_reset_with_timeout = (reset_with_timeout != 0u) ? 1u : 0u;
    wire_timeout_flag = 0u;
}

uint8_t Wire_getWireTimeoutFlag(void) STC_WIRE_REENTRANT
{
    return wire_timeout_flag;
}

void Wire_clearWireTimeoutFlag(void) STC_WIRE_REENTRANT
{
    wire_timeout_flag = 0u;
}

void Wire_beginTransmission(uint8_t address) STC_WIRE_REENTRANT
{
    if (wire_slave) { wire_last_error = WIRE_STATUS_OTHER_ERROR; return; }
    wire_ensure_initialized();
    wire_tx_address = address;
    wire_tx_length = 0u;
    wire_tx_overflow = 0u;
    wire_transmitting = 1u;
}

size_t Wire_write(uint8_t value) STC_WIRE_REENTRANT
{
    if (wire_transmitting == 0u) {
        return 0u;
    }
    if (wire_tx_length >= WIRE_BUFFER_LENGTH) {
        wire_tx_overflow = 1u;
        return 0u;
    }
    wire_tx_buffer[wire_tx_length++] = value;
    return 1u;
}

static uint8_t wire_end_transmission(uint8_t send_stop) STC_WIRE_REENTRANT
{
    uint8_t index;
    uint8_t byte_status;
    uint8_t status;

    if (wire_transmitting == 0u) {
        return WIRE_STATUS_OTHER_ERROR;
    }
    wire_transmitting = 0u;
    wire_ensure_initialized();

    if (wire_tx_address > 0x7fu) return WIRE_STATUS_OTHER_ERROR;

    if (wire_tx_overflow != 0u) {
        wire_tx_length = 0u;
        if (wire_bus_held != 0u) {
            (void)wire_stop_condition();
        }
        return WIRE_STATUS_DATA_TOO_LONG;
    }

    status = wire_start_condition();
    if (status != WIRE_STATUS_SUCCESS) {
        wire_tx_length = 0u;
        return status;
    }

    byte_status = wire_write_byte((uint8_t)(wire_tx_address << 1));
    if (byte_status != WIRE_INTERNAL_ACK) {
        status = (byte_status == WIRE_INTERNAL_TIMEOUT) ?
            WIRE_STATUS_TIMEOUT : WIRE_STATUS_ADDRESS_NACK;
        (void)wire_stop_condition();
        wire_tx_length = 0u;
        return status;
    }

    for (index = 0u; index < wire_tx_length; ++index) {
        byte_status = wire_write_byte(wire_tx_buffer[index]);
        if (byte_status != WIRE_INTERNAL_ACK) {
            status = (byte_status == WIRE_INTERNAL_TIMEOUT) ?
                WIRE_STATUS_TIMEOUT : WIRE_STATUS_DATA_NACK;
            (void)wire_stop_condition();
            wire_tx_length = 0u;
            return status;
        }
    }
    wire_tx_length = 0u;

    if (send_stop != 0u) {
        status = wire_stop_condition();
    } else {
        wire_release_sda();
        wire_bus_held = 1u;
        status = WIRE_STATUS_SUCCESS;
    }
    return status;
}

uint8_t Wire_endTransmission(void) STC_WIRE_REENTRANT
{
    return Wire_endTransmissionStop(1u);
}

uint8_t Wire_endTransmissionStop(uint8_t send_stop) STC_WIRE_REENTRANT
{
    wire_last_error = wire_end_transmission(send_stop);
    return wire_last_error;
}

uint8_t Wire_requestFromStop(uint8_t address, uint8_t quantity,
                             uint8_t send_stop) STC_WIRE_REENTRANT
{
    uint8_t byte_status;
    uint8_t index;
    uint8_t value;

    if (wire_slave) { wire_last_error = WIRE_STATUS_OTHER_ERROR; return 0u; }

    wire_ensure_initialized();
    wire_rx_index = 0u;
    wire_rx_length = 0u;
    wire_last_error = WIRE_STATUS_SUCCESS;
    if (address > 0x7fu) {
        wire_last_error = WIRE_STATUS_OTHER_ERROR;
        return 0;
    }
    if (quantity == 0u) {
        return 0u;
    }
    if (quantity > WIRE_BUFFER_LENGTH) {
        quantity = WIRE_BUFFER_LENGTH;
    }

    wire_last_error = wire_start_condition();
    if (wire_last_error != WIRE_STATUS_SUCCESS) {
        return 0u;
    }
    byte_status = wire_write_byte((uint8_t)(((address & 0x7fu) << 1) | 1u));
    if (byte_status != WIRE_INTERNAL_ACK) {
        wire_last_error = byte_status == WIRE_INTERNAL_TIMEOUT ?
            WIRE_STATUS_TIMEOUT : WIRE_STATUS_ADDRESS_NACK;
        (void)wire_stop_condition();
        return 0u;
    }

    for (index = 0u; index < quantity; ++index) {
        if (wire_read_byte((index + 1u < quantity) ? 1u : 0u, &value) == 0u) {
            wire_last_error = WIRE_STATUS_TIMEOUT;
            (void)wire_stop_condition();
            return wire_rx_length;
        }
        wire_rx_buffer[wire_rx_length++] = value;
    }

    if (send_stop != 0u) {
        wire_last_error = wire_stop_condition();
    } else {
        wire_release_sda();
        wire_bus_held = 1u;
    }
    return wire_rx_length;
}

uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity) STC_WIRE_REENTRANT
{
    return Wire_requestFromStop(address, quantity, 1u);
}

uint8_t Wire_requestFromInternal(uint8_t address, uint8_t quantity,
                                 uint32_t internal_address,
                                 uint8_t internal_address_size,
                                 uint8_t send_stop) STC_WIRE_REENTRANT
{
    uint8_t status;

    if (internal_address_size > 3u) {
        internal_address_size = 3u;
    }
    if (internal_address_size != 0u) {
        Wire_beginTransmission(address);
        if (internal_address_size >= 3u) {
            (void)Wire_write((uint8_t)(internal_address >> 16));
        }
        if (internal_address_size >= 2u) {
            (void)Wire_write((uint8_t)(internal_address >> 8));
        }
        (void)Wire_write((uint8_t)internal_address);

        status = Wire_endTransmissionStop(0u);
        if (status != WIRE_STATUS_SUCCESS) {
            return 0u;
        }
    }

    return Wire_requestFromStop(address, quantity, send_stop);
}

int Wire_available(void) STC_WIRE_REENTRANT
{
    return (int)(wire_rx_length - wire_rx_index);
}

int Wire_peek(void) STC_WIRE_REENTRANT
{
    if (wire_rx_index >= wire_rx_length) {
        return -1;
    }
    return (int)wire_rx_buffer[wire_rx_index];
}

int Wire_read(void) STC_WIRE_REENTRANT
{
    if (wire_rx_index >= wire_rx_length) {
        return -1;
    }
    return (int)wire_rx_buffer[wire_rx_index++];
}
