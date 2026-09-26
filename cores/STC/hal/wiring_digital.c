#include "Arduino.h"
#include "stc_sfr.h"
#include "wiring_digital_private.h"
#include "wiring_pwm_private.h"

/* Internal use only, after checking the encoded bit or validating the pin.
 * The public digitalPinToBitMask macro also accepts invalid encodings; using
 * it here made SDCC repeat those checks in every GPIO call. */
#define STC_VALID_PIN_MASK(pin) ((uint8_t)(1u << ((pin) & 7u)))

static uint8_t stc_critical_enter(void)
{
    uint8_t enabled = (uint8_t)(IE & STC_IE_EA);
    IE &= (uint8_t)~STC_IE_EA;
    return enabled;
}

static void stc_critical_leave(uint8_t enabled)
{
    if (enabled != 0u) {
        IE |= STC_IE_EA;
    }
}

uint8_t digitalPinIsValid(uint8_t pin)
{
    uint8_t port = (uint8_t)(pin >> 4);
    uint8_t mask;

    if ((pin & 0x0fu) > 7u) {
        return 0u;
    }
    mask = STC_VALID_PIN_MASK(pin);

    switch (port) {
    case 0u:
#ifdef PIN_VALID_MASK_P0
# if PIN_VALID_MASK_P0 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P0 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P0 & mask) != 0u) ? 1u : 0u;
# endif
#else
        return 1u;
#endif
    case 1u:
#ifdef PIN_VALID_MASK_P1
# if PIN_VALID_MASK_P1 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P1 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P1 & mask) != 0u) ? 1u : 0u;
# endif
#else
        return 1u;
#endif
    case 2u:
#ifdef PIN_VALID_MASK_P2
# if PIN_VALID_MASK_P2 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P2 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P2 & mask) != 0u) ? 1u : 0u;
# endif
#else
        return 1u;
#endif
    case 3u:
#ifdef PIN_VALID_MASK_P3
# if PIN_VALID_MASK_P3 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P3 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P3 & mask) != 0u) ? 1u : 0u;
# endif
#else
        return 1u;
#endif
#if STC_CORE_HAS_PORT4
    case 4u:
# ifdef PIN_VALID_MASK_P4
#  if PIN_VALID_MASK_P4 == 0x00U
        return 0u;
#  elif PIN_VALID_MASK_P4 == 0xffU
        return 1u;
#  else
        return (((uint8_t)PIN_VALID_MASK_P4 & mask) != 0u) ? 1u : 0u;
#  endif
# else
        return 1u;
# endif
#endif
#if STC_CORE_HAS_PORT5
    case 5u:
# ifdef PIN_VALID_MASK_P5
#  if PIN_VALID_MASK_P5 == 0x00U
        return 0u;
#  elif PIN_VALID_MASK_P5 == 0xffU
        return 1u;
#  else
        return (((uint8_t)PIN_VALID_MASK_P5 & mask) != 0u) ? 1u : 0u;
#  endif
# else
        return 1u;
# endif
#endif
#if STC_CORE_HAS_PORT6
    case 6u:
# ifdef PIN_VALID_MASK_P6
#  if PIN_VALID_MASK_P6 == 0x00U
        return 0u;
#  elif PIN_VALID_MASK_P6 == 0xffU
        return 1u;
#  else
        return (((uint8_t)PIN_VALID_MASK_P6 & mask) != 0u) ? 1u : 0u;
#  endif
# else
        return 1u;
# endif
#endif
#if STC_CORE_HAS_PORT7
    case 7u:
# ifdef PIN_VALID_MASK_P7
#  if PIN_VALID_MASK_P7 == 0x00U
        return 0u;
#  elif PIN_VALID_MASK_P7 == 0xffU
        return 1u;
#  else
        return (((uint8_t)PIN_VALID_MASK_P7 & mask) != 0u) ? 1u : 0u;
#  endif
# else
        return 1u;
# endif
#endif
#if STC_CORE_HAS_PORT8
    case 8u:
# if PIN_VALID_MASK_P8 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P8 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P8 & mask) != 0u) ? 1u : 0u;
# endif
#endif
#if STC_CORE_HAS_PORT9
    case 9u:
# if PIN_VALID_MASK_P9 == 0x00U
        return 0u;
# elif PIN_VALID_MASK_P9 == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_P9 & mask) != 0u) ? 1u : 0u;
# endif
#endif
#if STC_CORE_HAS_PORTA
    case 10u:
# if PIN_VALID_MASK_PA == 0x00U
        return 0u;
# elif PIN_VALID_MASK_PA == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_PA & mask) != 0u) ? 1u : 0u;
# endif
#endif
#if STC_CORE_HAS_PORTB
    case 11u:
# if PIN_VALID_MASK_PB == 0x00U
        return 0u;
# elif PIN_VALID_MASK_PB == 0xffU
        return 1u;
# else
        return (((uint8_t)PIN_VALID_MASK_PB & mask) != 0u) ? 1u : 0u;
# endif
#endif
    default:
        return 0u;
    }
}

static uint8_t stc_port_read(uint8_t port)
{
    switch (port) {
    case 0u: return P0;
    case 1u: return P1;
    case 2u: return P2;
    case 3u: return P3;
#if STC_CORE_HAS_PORT4
    case 4u: return P4;
#endif
#if STC_CORE_HAS_PORT5
    case 5u: return P5;
#endif
#if STC_CORE_HAS_PORT6
    case 6u: return P6;
#endif
#if STC_CORE_HAS_PORT7
    case 7u: return P7;
#endif
#if STC_CORE_HAS_PORT8
    case 8u: return P8IN;
#endif
#if STC_CORE_HAS_PORT9
    case 9u: return P9IN;
#endif
#if STC_CORE_HAS_PORTA
    case 10u: return PAIN;
#endif
#if STC_CORE_HAS_PORTB
    case 11u: return PBIN;
#endif
    default: return 0xffu;
    }
}

static void stc_port_latch_write(uint8_t port, uint8_t mask, uint8_t high)
{
    uint8_t interrupt_state = stc_critical_enter();

    switch (port) {
    case 0u:
        if (high != 0u) { P0 |= mask; } else { P0 &= (uint8_t)~mask; }
        break;
    case 1u:
        if (high != 0u) { P1 |= mask; } else { P1 &= (uint8_t)~mask; }
        break;
    case 2u:
        if (high != 0u) { P2 |= mask; } else { P2 &= (uint8_t)~mask; }
        break;
    case 3u:
        if (high != 0u) { P3 |= mask; } else { P3 &= (uint8_t)~mask; }
        break;
#if STC_CORE_HAS_PORT4
    case 4u:
        if (high != 0u) { P4 |= mask; } else { P4 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT5
    case 5u:
        if (high != 0u) { P5 |= mask; } else { P5 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT6
    case 6u:
        if (high != 0u) { P6 |= mask; } else { P6 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT7
    case 7u:
        if (high != 0u) { P7 |= mask; } else { P7 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT8
    case 8u:
        if (high != 0u) { P8SETB = mask; } else { P8CLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORT9
    case 9u:
        if (high != 0u) { P9SETB = mask; } else { P9CLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORTA
    case 10u:
        if (high != 0u) { PASETB = mask; } else { PACLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORTB
    case 11u:
        if (high != 0u) { PBSETB = mask; } else { PBCLRB = mask; }
        break;
#endif
    default: break;
    }

    stc_critical_leave(interrupt_state);
}

static uint8_t stc_pin_is_input(uint8_t port, uint8_t mask)
{
    return ((__stc_digital_input_pins[port] & mask) != 0u) ? 1u : 0u;
}

static void stc_set_pin_is_input(uint8_t port, uint8_t mask, uint8_t is_input)
{
    uint8_t interrupt_state = stc_critical_enter();

    if (is_input != 0u) {
        __stc_digital_input_pins[port] |= mask;
    } else {
        __stc_digital_input_pins[port] &= (uint8_t)~mask;
    }

    stc_critical_leave(interrupt_state);
}

#if STC_CORE_HAS_PORT_MODE
#define STC_APPLY_MODE(port_name)                 \
    do {                                          \
        if (m1 != 0u) {                           \
            port_name##M1 |= mask;                \
        } else {                                  \
            port_name##M1 &= (uint8_t)~mask;      \
        }                                         \
        if (m0 != 0u) {                           \
            port_name##M0 |= mask;                \
        } else {                                  \
            port_name##M0 &= (uint8_t)~mask;      \
        }                                         \
    } while (0)

static void stc_port_set_mode(uint8_t port, uint8_t mask, uint8_t m1, uint8_t m0)
{
    uint8_t interrupt_state = stc_critical_enter();

    switch (port) {
    case 0u: STC_APPLY_MODE(P0); break;
    case 1u: STC_APPLY_MODE(P1); break;
    case 2u: STC_APPLY_MODE(P2); break;
    case 3u: STC_APPLY_MODE(P3); break;
#if STC_CORE_HAS_PORT4
    case 4u: STC_APPLY_MODE(P4); break;
#endif
#if STC_CORE_HAS_PORT5
    case 5u: STC_APPLY_MODE(P5); break;
#endif
#if STC_CORE_HAS_PORT6
    case 6u: STC_APPLY_MODE(P6); break;
#endif
#if STC_CORE_HAS_PORT7
    case 7u: STC_APPLY_MODE(P7); break;
#endif
#if STC_CORE_HAS_PORT8
    case 8u: STC_APPLY_MODE(P8); break;
#endif
#if STC_CORE_HAS_PORT9
    case 9u: STC_APPLY_MODE(P9); break;
#endif
#if STC_CORE_HAS_PORTA
    case 10u: STC_APPLY_MODE(PA); break;
#endif
#if STC_CORE_HAS_PORTB
    case 11u: STC_APPLY_MODE(PB); break;
#endif
    default: break;
    }

    stc_critical_leave(interrupt_state);
}
#undef STC_APPLY_MODE
#endif

#if STC_CORE_HAS_SEPARATE_PULLUP
#define STC_APPLY_PULLUP(port_name)             \
    do {                                         \
        if (enabled != 0u) {                     \
            port_name##PU |= mask;               \
        } else {                                 \
            port_name##PU &= (uint8_t)~mask;     \
        }                                        \
    } while (0)

static void stc_port_set_pullup(uint8_t port, uint8_t mask, uint8_t enabled)
{
    uint8_t interrupt_state = stc_critical_enter();
    uint8_t window = P_SW2;

    /* Small STC32CL parts need the same XFR gate as other STC32 parts,
     * but do not leave it enabled globally during startup. */
    P_SW2 |= STC_P_SW2_EAXFR;

    switch (port) {
    case 0u: STC_APPLY_PULLUP(P0); break;
    case 1u: STC_APPLY_PULLUP(P1); break;
    case 2u: STC_APPLY_PULLUP(P2); break;
    case 3u: STC_APPLY_PULLUP(P3); break;
#if STC_CORE_HAS_PORT4
    case 4u: STC_APPLY_PULLUP(P4); break;
#endif
#if STC_CORE_HAS_PORT5
    case 5u: STC_APPLY_PULLUP(P5); break;
#endif
#if STC_CORE_HAS_PORT6
    case 6u: STC_APPLY_PULLUP(P6); break;
#endif
#if STC_CORE_HAS_PORT7
    case 7u: STC_APPLY_PULLUP(P7); break;
#endif
#if STC_CORE_HAS_PORT8
    case 8u: STC_APPLY_PULLUP(P8); break;
#endif
#if STC_CORE_HAS_PORT9
    case 9u: STC_APPLY_PULLUP(P9); break;
#endif
#if STC_CORE_HAS_PORTA
    case 10u: STC_APPLY_PULLUP(PA); break;
#endif
#if STC_CORE_HAS_PORTB
    case 11u: STC_APPLY_PULLUP(PB); break;
#endif
    default: break;
    }

    P_SW2 = window;
    stc_critical_leave(interrupt_state);
}
#undef STC_APPLY_PULLUP
#endif

#if STC_VARIANT_PIN_ALIAS_GROUP_COUNT > 0
static void stc_release_physical_alias(uint8_t pin)
{
    uint8_t alias = (uint8_t)STC_VARIANT_PHYSICAL_ALIAS(pin);
    uint8_t port;
    uint8_t mask;

    if (alias == NOT_A_PIN) {
        return;
    }
    port = (uint8_t)(alias >> 4);
    mask = STC_VALID_PIN_MASK(alias);

#if STC_CORE_ADC_USES_P1ASF
    if (port == 1u) {
        uint8_t interrupt_state = stc_critical_enter();
        P1ASF &= (uint8_t)~mask;
        stc_critical_leave(interrupt_state);
    }
#endif

#if STC_CORE_HAS_PORT_MODE
    /* Never leave both port cells driving one package pad. */
# if STC_CORE_HAS_SEPARATE_PULLUP
    stc_port_set_pullup(port, mask, 0u);
# endif
    stc_port_set_mode(port, mask, 1u, 0u);
    stc_port_latch_write(port, mask, 1u);
#else
    stc_port_latch_write(port, mask, 1u);
#endif
    stc_set_pin_is_input(port, mask, 1u);
}
#endif

void digitalWrite(uint8_t pin, uint8_t value) STC_REENTRANT
{
    uint8_t port;
    uint8_t mask;

    if (digitalPinIsValid(pin) == 0u) {
        return;
    }

#if STC_CORE_PWM_LAYOUT
    if (stc_pwm_active) stc_pwm_detach(pin);
#endif
    port = (uint8_t)(pin >> 4);
    mask = STC_VALID_PIN_MASK(pin);
    if (stc_pin_is_input(port, mask) != 0u) {
#if STC_CORE_HAS_PORT_MODE
# if STC_CORE_HAS_SEPARATE_PULLUP
        stc_port_set_mode(port, mask, 1u, 0u);
        stc_port_latch_write(port, mask, 1u);
        stc_port_set_pullup(port, mask, (value != LOW) ? 1u : 0u);
# else
        if (value != LOW) {
            /* Arduino HIGH on an input enables the STC quasi-mode pull-up. */
            stc_port_latch_write(port, mask, 1u);
            stc_port_set_mode(port, mask, 0u, 0u);
        } else {
            /* Switch to input-only before releasing the latch. */
            stc_port_set_mode(port, mask, 1u, 0u);
            stc_port_latch_write(port, mask, 1u);
        }
# endif
#else
        /* Classic quasi ports cannot disable their weak pull-up without also
         * driving low.  Keeping the latch high is the only safe input state;
         * P0 is open-drain and is therefore released here as well. */
        stc_port_latch_write(port, mask, 1u);
#endif
        return;
    }

    stc_port_latch_write(port, mask, (value != LOW) ? 1u : 0u);
}

int digitalRead(uint8_t pin)
{
    uint8_t port;
    uint8_t mask;

    if (digitalPinIsValid(pin) == 0u) {
        return LOW;
    }

    port = (uint8_t)(pin >> 4);
    mask = STC_VALID_PIN_MASK(pin);
    return ((stc_port_read(port) & mask) != 0u) ? HIGH : LOW;
}

void pinMode(uint8_t pin, uint8_t mode) STC_REENTRANT
{
    uint8_t port;
    uint8_t mask;

    if (digitalPinIsValid(pin) == 0u) {
        return;
    }
    if ((mode != INPUT) && (mode != OUTPUT) &&
        (mode != INPUT_PULLUP) && (mode != OUTPUT_OPEN_DRAIN) &&
        (mode != OUTPUT_QUASI)) {
        return;
    }

#if STC_CORE_PWM_LAYOUT
    if (stc_pwm_active) stc_pwm_detach(pin);
#endif
    port = (uint8_t)(pin >> 4);
    mask = STC_VALID_PIN_MASK(pin);

#if STC_CORE_ADC_USES_P1ASF
    if (port == 1u) {
        uint8_t interrupt_state = stc_critical_enter();
        P1ASF &= (uint8_t)~mask;
        stc_critical_leave(interrupt_state);
    }
#endif

#if STC_VARIANT_PIN_ALIAS_GROUP_COUNT > 0
    stc_release_physical_alias(pin);
#endif

#if STC_CORE_HAS_SEPARATE_PULLUP
    /* Every non-pull-up mode must release a previously enabled resistor. */
    stc_port_set_pullup(port, mask, 0u);
#endif

#if STC_CORE_HAS_PORT_MODE
    switch (mode) {
    case INPUT:             /* M1:M0 = 10, input-only */
        stc_port_set_mode(port, mask, 1u, 0u);
        stc_port_latch_write(port, mask, 1u);
        stc_set_pin_is_input(port, mask, 1u);
        break;
    case OUTPUT:            /* M1:M0 = 01, push-pull */
        stc_set_pin_is_input(port, mask, 0u);
        stc_port_set_mode(port, mask, 0u, 1u);
        break;
    case INPUT_PULLUP:      /* Input with the target's internal pull-up. */
        stc_port_set_mode(port, mask, 1u, 0u);
        stc_port_latch_write(port, mask, 1u);
#if STC_CORE_HAS_SEPARATE_PULLUP
        /* Targets with separate pull-up controls keep the cell high-Z. */
        stc_port_set_pullup(port, mask, 1u);
#else
        stc_port_set_mode(port, mask, 0u, 0u);
#endif
        stc_set_pin_is_input(port, mask, 1u);
        break;
    case OUTPUT_QUASI:      /* M1:M0 = 00, quasi-bidirectional output */
        stc_set_pin_is_input(port, mask, 0u);
        stc_port_set_mode(port, mask, 0u, 0u);
        break;
    default:                /* M1:M0 = 11, open drain */
        stc_port_set_mode(port, mask, 1u, 1u);
        stc_port_latch_write(port, mask, 1u);
        stc_set_pin_is_input(port, mask, 0u);
        break;
    }
#endif
}
