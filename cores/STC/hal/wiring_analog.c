#include "Arduino.h"
#include "stc_sfr.h"

#define STC_ADC_POWER_MASK 0x80u

# define STC_ADC_START_MASK   0x40u
# define STC_ADC_FLAG_MASK    0x20u
# define STC_ADC_CHANNEL_MASK 0x0fu

#if (F_CPU / 500UL) < 1000UL
# define STC_ADC_POWER_DELAY_LOOPS 1000U
#elif (F_CPU / 500UL) > 60000UL
# define STC_ADC_POWER_DELAY_LOOPS 60000U
#else
# define STC_ADC_POWER_DELAY_LOOPS ((uint16_t)(F_CPU / 500UL))
#endif

#if (F_CPU / 1000UL) < 1000UL
# define STC_ADC_TIMEOUT_LOOPS 1000U
#elif (F_CPU / 1000UL) > 60000UL
# define STC_ADC_TIMEOUT_LOOPS 60000U
#else
# define STC_ADC_TIMEOUT_LOOPS ((uint16_t)(F_CPU / 1000UL))
#endif

/* Arduino libraries conventionally expect a 10-bit analogRead() result. */
static uint8_t stc_adc_requested_bits = 10u;

#if STC_CORE_HAS_ADC
static uint8_t stc_adc_powered;

static void stc_adc_wait_after_control_write(void)
{
    /* STC requires at least four CPU clocks before ADC_CONTR is read. */
    __asm
        nop
        nop
        nop
        nop
    __endasm;
}

static void stc_adc_power_up_delay(void)
{
    uint16_t loops = STC_ADC_POWER_DELAY_LOOPS;

    /* This independent busy loop also completes when interrupts are disabled. */
    while (loops != 0u) {
        --loops;
        __asm
            nop
        __endasm;
    }
}


static void stc_adc_prepare(void)
{
    /* Keep reserved bits, select right alignment and the conservative /16 ADC
     * clock. The documented reset ADCTIM value already has SMPDUTY == 10. */
    ADCCFG = (uint8_t)((ADCCFG & 0xc0u) | 0x20u | 0x0fu);

    if (stc_adc_powered == 0u) {
        ADC_CONTR = STC_ADC_POWER_MASK;
        stc_adc_power_up_delay();
        stc_adc_powered = 1u;
    }
}

static void stc_adc_select_pin(uint8_t pin)
{
    pinMode(pin, INPUT);

    (void)pin;
}

static uint16_t stc_adc_native_result(void)
{
    return ((uint16_t)ADC_RES << 8) | ADC_RESL;
}

static int stc_adc_scale_result(uint16_t value)
{
    uint8_t bits = stc_adc_requested_bits;

    value &= (uint16_t)((1u << STC_CORE_ADC_NATIVE_BITS) - 1u);
    if (bits < STC_CORE_ADC_NATIVE_BITS) {
        value >>= (STC_CORE_ADC_NATIVE_BITS - bits);
    } else if (bits > STC_CORE_ADC_NATIVE_BITS) {
        value <<= (bits - STC_CORE_ADC_NATIVE_BITS);
    }
    return (int)value;
}
#endif

int analogRead(uint8_t pin)
{
#if STC_CORE_HAS_ADC
    uint8_t channel = (uint8_t)STC_VARIANT_ADC_PIN_TO_CHANNEL(pin);
    uint8_t control;
    uint16_t timeout;
    uint16_t result;

    if ((channel == NOT_AN_ADC_CHANNEL) || (digitalPinIsValid(pin) == 0u)) {
        return -1;
    }

    stc_adc_select_pin(pin);
    stc_adc_prepare();

    control = (uint8_t)(STC_ADC_POWER_MASK |
                        (channel & STC_ADC_CHANNEL_MASK));
    ADC_CONTR = (uint8_t)(control | STC_ADC_START_MASK);
    stc_adc_wait_after_control_write();

    timeout = STC_ADC_TIMEOUT_LOOPS;
    while (((ADC_CONTR & STC_ADC_FLAG_MASK) == 0u) && (timeout != 0u)) {
        --timeout;
    }
    if (timeout == 0u) {
        ADC_CONTR = control;
        return -1;
    }

    result = stc_adc_native_result();
    ADC_CONTR = control; /* Stop and clear the completion flag. */
    return stc_adc_scale_result(result);
#else
    (void)pin;
    return -1;
#endif
}

void analogReference(uint8_t mode)
{
    /* Supported STC ADCs use their ADC supply/reference input. DEFAULT is the
     * only portable mode; unsupported modes intentionally leave hardware alone. */
    (void)mode;
}

void analogReadResolution(uint8_t bits)
{
    if (bits < 1u) {
        bits = 1u;
    } else if (bits > 15u) {
        /* SDCC's Arduino int is signed 16-bit, so 15 bits is the safe maximum. */
        bits = 15u;
    }
    stc_adc_requested_bits = bits;
}
