// Generated support header. Pin encoding is kept compatible with the original core.
#ifndef STC_VARIANT_PINS_COMMON_H
#define STC_VARIANT_PINS_COMMON_H

#ifndef NOT_A_PIN
#define NOT_A_PIN 0xFF
#endif
#ifndef NOT_A_PORT
#define NOT_A_PORT 0xFF
#endif
#ifndef NOT_AN_ANALOG_INPUT
#define NOT_AN_ANALOG_INPUT 0xFF
#endif
#ifndef NOT_AN_ADC_CHANNEL
#define NOT_AN_ADC_CHANNEL 0xFF
#endif
#define STC_PORT_PIN(port, bit) ((((port) & 0x0F) << 4) | ((bit) & 0x07))
#define STC_PIN_PORT(pin) (((pin) == NOT_A_PIN) ? NOT_A_PORT : (((pin) >> 4) & 0x0F))
#define STC_PIN_BIT(pin) ((pin) & 0x07)
#define STC_PIN_BIT_MASK(pin)   (((pin) == NOT_A_PIN || (((pin) & 0x0F) > 7)) ? 0U : (1U << STC_PIN_BIT(pin)))
#define STC_PIN_ENCODING_LIMIT 0xB8
#define STC_NUM_PIN_CODES STC_PIN_ENCODING_LIMIT

#define P0_0 STC_PORT_PIN(0, 0)
#define P0_1 STC_PORT_PIN(0, 1)
#define P0_2 STC_PORT_PIN(0, 2)
#define P0_3 STC_PORT_PIN(0, 3)
#define P0_4 STC_PORT_PIN(0, 4)
#define P0_5 STC_PORT_PIN(0, 5)
#define P0_6 STC_PORT_PIN(0, 6)
#define P0_7 STC_PORT_PIN(0, 7)
#define P1_0 STC_PORT_PIN(1, 0)
#define P1_1 STC_PORT_PIN(1, 1)
#define P1_2 STC_PORT_PIN(1, 2)
#define P1_3 STC_PORT_PIN(1, 3)
#define P1_4 STC_PORT_PIN(1, 4)
#define P1_5 STC_PORT_PIN(1, 5)
#define P1_6 STC_PORT_PIN(1, 6)
#define P1_7 STC_PORT_PIN(1, 7)
#define P2_0 STC_PORT_PIN(2, 0)
#define P2_1 STC_PORT_PIN(2, 1)
#define P2_2 STC_PORT_PIN(2, 2)
#define P2_3 STC_PORT_PIN(2, 3)
#define P2_4 STC_PORT_PIN(2, 4)
#define P2_5 STC_PORT_PIN(2, 5)
#define P2_6 STC_PORT_PIN(2, 6)
#define P2_7 STC_PORT_PIN(2, 7)
#define P3_0 STC_PORT_PIN(3, 0)
#define P3_1 STC_PORT_PIN(3, 1)
#define P3_2 STC_PORT_PIN(3, 2)
#define P3_3 STC_PORT_PIN(3, 3)
#define P3_4 STC_PORT_PIN(3, 4)
#define P3_5 STC_PORT_PIN(3, 5)
#define P3_6 STC_PORT_PIN(3, 6)
#define P3_7 STC_PORT_PIN(3, 7)
#define P4_0 STC_PORT_PIN(4, 0)
#define P4_1 STC_PORT_PIN(4, 1)
#define P4_2 STC_PORT_PIN(4, 2)
#define P4_3 STC_PORT_PIN(4, 3)
#define P4_4 STC_PORT_PIN(4, 4)
#define P4_5 STC_PORT_PIN(4, 5)
#define P4_6 STC_PORT_PIN(4, 6)
#define P4_7 STC_PORT_PIN(4, 7)
#define P5_0 STC_PORT_PIN(5, 0)
#define P5_1 STC_PORT_PIN(5, 1)
#define P5_2 STC_PORT_PIN(5, 2)
#define P5_3 STC_PORT_PIN(5, 3)
#define P5_4 STC_PORT_PIN(5, 4)
#define P5_5 STC_PORT_PIN(5, 5)
#define P5_6 STC_PORT_PIN(5, 6)
#define P5_7 STC_PORT_PIN(5, 7)
#define P6_0 STC_PORT_PIN(6, 0)
#define P6_1 STC_PORT_PIN(6, 1)
#define P6_2 STC_PORT_PIN(6, 2)
#define P6_3 STC_PORT_PIN(6, 3)
#define P6_4 STC_PORT_PIN(6, 4)
#define P6_5 STC_PORT_PIN(6, 5)
#define P6_6 STC_PORT_PIN(6, 6)
#define P6_7 STC_PORT_PIN(6, 7)
#define P7_0 STC_PORT_PIN(7, 0)
#define P7_1 STC_PORT_PIN(7, 1)
#define P7_2 STC_PORT_PIN(7, 2)
#define P7_3 STC_PORT_PIN(7, 3)
#define P7_4 STC_PORT_PIN(7, 4)
#define P7_5 STC_PORT_PIN(7, 5)
#define P7_6 STC_PORT_PIN(7, 6)
#define P7_7 STC_PORT_PIN(7, 7)
#define P8_0 STC_PORT_PIN(8, 0)
#define P8_1 STC_PORT_PIN(8, 1)
#define P8_2 STC_PORT_PIN(8, 2)
#define P8_3 STC_PORT_PIN(8, 3)
#define P8_4 STC_PORT_PIN(8, 4)
#define P8_5 STC_PORT_PIN(8, 5)
#define P8_6 STC_PORT_PIN(8, 6)
#define P8_7 STC_PORT_PIN(8, 7)
#define P9_0 STC_PORT_PIN(9, 0)
#define P9_1 STC_PORT_PIN(9, 1)
#define P9_2 STC_PORT_PIN(9, 2)
#define P9_3 STC_PORT_PIN(9, 3)
#define P9_4 STC_PORT_PIN(9, 4)
#define P9_5 STC_PORT_PIN(9, 5)
#define P9_6 STC_PORT_PIN(9, 6)
#define P9_7 STC_PORT_PIN(9, 7)
#define PA_0 STC_PORT_PIN(10, 0)
#define PA_1 STC_PORT_PIN(10, 1)
#define PA_2 STC_PORT_PIN(10, 2)
#define PA_3 STC_PORT_PIN(10, 3)
#define PA_4 STC_PORT_PIN(10, 4)
#define PA_5 STC_PORT_PIN(10, 5)
#define PA_6 STC_PORT_PIN(10, 6)
#define PA_7 STC_PORT_PIN(10, 7)
#define PB_0 STC_PORT_PIN(11, 0)
#define PB_1 STC_PORT_PIN(11, 1)
#define PB_2 STC_PORT_PIN(11, 2)
#define PB_3 STC_PORT_PIN(11, 3)
#define PB_4 STC_PORT_PIN(11, 4)
#define PB_5 STC_PORT_PIN(11, 5)
#define PB_6 STC_PORT_PIN(11, 6)
#define PB_7 STC_PORT_PIN(11, 7)

#define LED_BUILTIN NOT_A_PIN
#define NUM_DIGITAL_PINS STC_NUM_PIN_CODES
#ifndef NUM_ANALOG_INPUTS
# define NUM_ANALOG_INPUTS 0
#endif
#ifndef analogInputToDigitalPin
# define analogInputToDigitalPin(index) (NOT_A_PIN)
#endif
#ifndef digitalPinToAnalogInput
# define digitalPinToAnalogInput(pin) (NOT_AN_ANALOG_INPUT)
#endif
#ifndef STC_VARIANT_ADC_PIN_TO_CHANNEL
# define STC_VARIANT_ADC_PIN_TO_CHANNEL(pin) (NOT_AN_ADC_CHANNEL)
#endif
#ifndef STC_VARIANT_FIRST_ANALOG_PIN
# define STC_VARIANT_FIRST_ANALOG_PIN NOT_A_PIN
#endif
#ifndef STC_VARIANT_LAST_ANALOG_PIN
# define STC_VARIANT_LAST_ANALOG_PIN NOT_A_PIN
#endif
#ifndef STC_VARIANT_PINS_SHARE_PHYSICAL_PAD
# define STC_VARIANT_PINS_SHARE_PHYSICAL_PAD(left, right) (0)
#endif
#ifndef STC_VARIANT_PHYSICAL_ALIAS
# define STC_VARIANT_PHYSICAL_ALIAS(pin) (NOT_A_PIN)
#endif
#ifndef digitalPinsSharePhysicalPad
# define digitalPinsSharePhysicalPad(left, right)     STC_VARIANT_PINS_SHARE_PHYSICAL_PAD((left), (right))
#endif
#ifndef digitalPinToPhysicalAlias
# define digitalPinToPhysicalAlias(pin) STC_VARIANT_PHYSICAL_ALIAS(pin)
#endif

#if STC_CORE_WIRE_LAYOUT != 0
# define PIN_WIRE_SDA P3_3
# define PIN_WIRE_SCL P3_2
#else
# define PIN_WIRE_SDA P3_2
# define PIN_WIRE_SCL P3_3
#endif
#define SDA PIN_WIRE_SDA
#define SCL PIN_WIRE_SCL

#if STC_CORE_BUS_LAYOUT == 1
# define PIN_SPI_MOSI P1_3
# define PIN_SPI_MISO P1_4
# define PIN_SPI_SCK  P1_5
# if (PIN_VALID_MASK_P1 & 0x01U)
#  define PIN_SPI_SS  P1_0
# else
#  define PIN_SPI_SS  P1_2
# endif
#else
# define PIN_SPI_MOSI P3_2
# define PIN_SPI_MISO P3_3
# define PIN_SPI_SCK  P3_4
# define PIN_SPI_SS   P3_5
#endif
#ifdef __cplusplus
#include <stdint.h>
// Public Arduino pin names must permit parameter names such as MOSI in libraries.
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t SS   = PIN_SPI_SS;
#else
// Native C drivers retain preprocessor constants for their pin configuration.
#define MOSI PIN_SPI_MOSI
#define MISO PIN_SPI_MISO
#define SCK  PIN_SPI_SCK
#define SS   PIN_SPI_SS
#endif

#ifndef digitalPinToPort
#define digitalPinToPort(pin) STC_PIN_PORT(pin)
#endif
#ifndef digitalPinToBitMask
#define digitalPinToBitMask(pin) STC_PIN_BIT_MASK(pin)
#endif

#endif
