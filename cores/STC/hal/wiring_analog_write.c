#include "wiring_pwm_private.h"
#include "stc_sfr.h"

static uint8_t stc_pwm_error;

#if STC_CORE_PWM_LAYOUT
/* Positive outputs only, in channel order, four routes per channel.
 * When one pin has two channels, the first entry is its stable assignment.
 * Filter every entry through the selected model's bonded-pin mask.
 */
static const uint8_t stc_pwm_pins[32] = {
#if STC_CORE_PWM_LAYOUT == 2
    P1_0, P0_0, P2_0, NOT_A_PIN,
    P1_2, P0_2, P2_2, NOT_A_PIN,
    P1_4, P0_4, P2_4, NOT_A_PIN,
    P1_6, P0_6, P2_6, NOT_A_PIN,
    P0_1, P1_1, P2_1, P5_0,
    P0_3, P1_3, P2_3, P5_1,
    P0_5, P1_5, P2_5, P5_2,
    P0_7, P1_7, P2_7, P5_3
#else
    P1_0, P2_0, P6_0, NOT_A_PIN,
# if (PIN_VALID_MASK_P1 & 4u)
    P1_2, P2_2, P6_2, NOT_A_PIN,
# else
    P5_4, P2_2, P6_2, NOT_A_PIN,
# endif
    P1_4, P2_4, P6_4, NOT_A_PIN,
    P1_6, P2_6, P6_6, P3_4,
    P2_0, P1_7, P0_0, P7_4,
    P2_1, P5_4, P0_1, P7_5,
    P2_2, P3_3, P0_2, P7_6,
    P2_3, P3_4, P0_3, P7_7
#endif
};

static uint8_t stc_pwm_route(uint8_t pin)
{
    uint8_t i;
    if (!digitalPinIsValid(pin)) return NOT_A_PIN;
    for (i = 0; i != 32u; ++i) if (stc_pwm_pins[i] == pin) return i;
    return NOT_A_PIN;
}
#endif

uint8_t analogWriteSupported(uint8_t pin) STC_REENTRANT
{
#if STC_CORE_PWM_LAYOUT
    return stc_pwm_route(pin) != NOT_A_PIN;
#else
    (void)pin;
    return 0;
#endif
}

uint8_t analogWriteConfigurationError(void) { return stc_pwm_error; }

uint8_t analogWriteChecked(uint8_t pin, int value) STC_REENTRANT
{
#if STC_CORE_PWM_LAYOUT
    uint8_t route, channel, local, base, eno, shift, enabled, window, i, active;
    uint16_t prescaler = (uint16_t)((F_CPU + 128000UL) / 256000UL - 1UL);
#endif
    if (!digitalPinIsValid(pin) || value < 0 || value > 255)
        return stc_pwm_error = STC_PWM_INVALID;
    if (!analogWriteSupported(pin)) return stc_pwm_error = STC_PWM_UNSUPPORTED;
#if STC_CORE_PWM_LAYOUT
    route = stc_pwm_route(pin);
    channel = route >> 2;
    local = channel & 3u;
    base = channel < 4u ? 0xc0u : 0xe0u;
    eno = channel < 4u ? 0xb1u : 0xb5u;
    shift = local * 2u;
    /* Bank ownership is exclusive to this API. Do not mix direct-register
     * PWM drivers with analogWrite; Timer0/Timer1 are never reconfigured.
     */
    enabled = IE & STC_IE_EA;
    IE &= (uint8_t)~STC_IE_EA;
    if (stc_pwm_owner[channel] && stc_pwm_owner[channel] != (uint8_t)(pin + 1u)) {
        if (enabled) IE |= STC_IE_EA;
        return stc_pwm_error = STC_PWM_BUSY;
    }
    if (!stc_pwm_owner[channel]) pinMode(pin, OUTPUT);
    if (value == 0 || value == 255) {
        digitalWrite(pin, value == 0 ? LOW : HIGH);
        if (enabled) IE |= STC_IE_EA;
        return stc_pwm_error = STC_PWM_OK;
    }
    window = P_SW2;
    P_SW2 |= STC_P_SW2_EAXFR;
    active = 0;
    for (i = channel & 4u; i < (uint8_t)((channel & 4u) + 4u); ++i)
        active |= stc_pwm_owner[i];
    if (!active) {
        STC_PWM_REG(base) = 0;
        STC_PWM_REG(base + 1u) = 0;
        STC_PWM_REG(base + 2u) = 0;
        STC_PWM_REG(base + 4u) = 0;
        STC_PWM_REG(base + 0x0cu) = 0;
        STC_PWM_REG(base + 0x0du) = 0;
        STC_PWM_REG(eno) = 0;
        STC_PWM_REG(base + 0x10u) = (uint8_t)(prescaler >> 8);
        STC_PWM_REG(base + 0x11u) = (uint8_t)prescaler;
        STC_PWM_REG(base + 0x12u) = 0;
        STC_PWM_REG(base + 0x13u) = 255;
        STC_PWM_REG(base + 0x14u) = 0;
        STC_PWM_REG(base + 0x1du) = 0x80u;
    }
    STC_PWM_REG(eno + 1u) = (STC_PWM_REG(eno + 1u) & (uint8_t)~(3u << shift)) |
                            ((route & 3u) << shift);
    STC_PWM_REG(base + 8u + local) = 0x68u;
    STC_PWM_REG(base + 0x15u + shift) = 0;
    STC_PWM_REG(base + 0x16u + shift) = (uint8_t)value;
    shift = (local & 1u) * 4u;
    STC_PWM_REG(base + 0x0cu + (local >> 1)) =
        (STC_PWM_REG(base + 0x0cu + (local >> 1)) & (uint8_t)~(15u << shift)) |
        (1u << shift);
    if (!active) {
        STC_PWM_REG(base + 7u) = 1u; /* load prescaler/ARR/CCR shadow */
        STC_PWM_REG(base) = 0x81u;
    }
    STC_PWM_REG(eno) |= (uint8_t)(1u << (local * 2u));
    stc_pwm_owner[channel] = pin + 1u;
    stc_pwm_active |= (uint8_t)(1u << channel);
    P_SW2 = window;
    if (enabled) IE |= STC_IE_EA;
#endif
    return stc_pwm_error = STC_PWM_OK;
}

void analogWrite(uint8_t pin, int value) STC_REENTRANT
{
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    if (analogWriteChecked(pin, value) == STC_PWM_UNSUPPORTED) {
        /* Standard Arduino digital fallback on pins with no PWM channel. */
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value < 128 ? LOW : HIGH);
    }
}
