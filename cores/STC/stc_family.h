#ifndef STC_CORE_FAMILY_H
#define STC_CORE_FAMILY_H

#define STC_ADC_LAYOUT_NONE                    0
/* USB SIE: 1=STC32G12K/AI8051U, 2=G144. CAN: 1=classic, 2=G144 FD. */
#ifndef STC_CORE_USB_LAYOUT
# define STC_CORE_USB_LAYOUT 0
#endif
#ifndef ARDUINO_USB_CDC_ON_BOOT
# define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#ifndef STC_USB_CDC_ONLY
# define STC_USB_CDC_ONLY 0
#endif
#ifndef STC_USB_HID_ONLY
# define STC_USB_HID_ONLY 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT && !STC_CORE_USB_LAYOUT
# error "USB CDC requires a variant with a native USB peripheral"
#endif
#ifndef STC_CORE_CAN_LAYOUT
# define STC_CORE_CAN_LAYOUT 0
#endif
#if STC_CORE_USB_LAYOUT < 0 || STC_CORE_USB_LAYOUT > 2 || \
    STC_CORE_CAN_LAYOUT < 0 || STC_CORE_CAN_LAYOUT > 2
# error "Unsupported USB/CAN register layout"
#endif
/* 1: STC32G/AI8051U E9/EA; 2: G144. */
#ifndef STC_CORE_MEMORY_TIMING_LAYOUT
# define STC_CORE_MEMORY_TIMING_LAYOUT 0
#endif
/* 1: audited STC32G classic SPI + 7EFE80 I2C controller. */
#ifndef STC_CORE_BUS_LAYOUT
# define STC_CORE_BUS_LAYOUT 0
#endif
/* Wire: 1=STC32 6-bit, 2=G144 14-bit. */
#ifndef STC_CORE_WIRE_LAYOUT
# define STC_CORE_WIRE_LAYOUT STC_CORE_BUS_LAYOUT
#endif
#if STC_CORE_WIRE_LAYOUT < 0 || STC_CORE_WIRE_LAYOUT > 2
# error "Unsupported STC Wire layout"
#endif
#if STC_CORE_MEMORY_TIMING_LAYOUT < 0 || STC_CORE_MEMORY_TIMING_LAYOUT > 2 || \
    STC_CORE_BUS_LAYOUT < 0 || STC_CORE_BUS_LAYOUT > 1
# error "Unsupported STC memory/bus register layout"
#endif
/* Audited advanced PWM routes: 1=STC32G, 2=AI8051U. */
#ifndef STC_CORE_PWM_LAYOUT
# define STC_CORE_PWM_LAYOUT 0
#endif
#if STC_CORE_PWM_LAYOUT < 0 || STC_CORE_PWM_LAYOUT > 2
# error "Unsupported PWM layout"
#endif
#define STC_ADC_LAYOUT_MODERN_BC_ADCCFG         4

/*
 * C++ translation units are lowered by the Clang STC frontend and then
 * rebuilt by SDCC at the bridge boundary.  Distinguish that frontend target
 * from the native SDCC predefined macros while accepting either as evidence
 * of the selected machine model.
 */
#if defined(__SDCC_mcs51) || defined(__STC_MCS51__) || defined(STC_EXECUTION_MODE_MCS51)
# error "MCS51 support has been removed; select an MCS251 board and compiler"
#endif
#if !defined(__SDCC_mcs251) && !defined(__STC_MCS251__)
# error "The STC core requires an MCS251 compiler target"
#endif

/*
 * Generated board definitions select the core family and hardware
 * capabilities explicitly.  Model-name macros remain available to SDK
 * headers and sketches, but the core never needs a per-model allow-list.
 */

#if (defined(STC_CORE_FAMILY_32) + defined(STC_CORE_FAMILY_AI8051U)) != 1
# error "Select exactly one supported STC core family: STC32 or AI8051U"
#endif

#if !defined(STC_CORE_HAS_PORT0) || !defined(STC_CORE_HAS_PORT1) || \
    !defined(STC_CORE_HAS_PORT2) || !defined(STC_CORE_HAS_PORT3) || \
    !defined(STC_CORE_HAS_PORT4) || !defined(STC_CORE_HAS_PORT5) || \
    !defined(STC_CORE_HAS_PORT6) || !defined(STC_CORE_HAS_PORT7) || \
    !defined(STC_CORE_HAS_PORT8) || !defined(STC_CORE_HAS_PORT9) || \
    !defined(STC_CORE_HAS_PORTA) || !defined(STC_CORE_HAS_PORTB) || \
    !defined(STC_CORE_HAS_PORT_MODE) || !defined(STC_CORE_TIMER1_IS_1T) || \
    !defined(STC_CORE_HAS_UART1) || !defined(STC_CORE_SERIAL_BUFFERED_RX) || \
    !defined(STC_CORE_HAS_ADC) || !defined(STC_CORE_ADC_LAYOUT) || \
    !defined(STC_CORE_HAS_SEPARATE_PULLUP) || \
    !defined(STC_CORE_ADC_NATIVE_BITS)
# error "Selected board is missing generated STC core capability flags"
#endif

#if (STC_CORE_HAS_PORT0 != 1) || (STC_CORE_HAS_PORT1 != 1) || \
    (STC_CORE_HAS_PORT2 != 1) || (STC_CORE_HAS_PORT3 != 1)
# error "The current STC core requires ports P0 through P3"
#endif

#if ((STC_CORE_HAS_PORT4 != 0) && (STC_CORE_HAS_PORT4 != 1)) || \
    ((STC_CORE_HAS_PORT5 != 0) && (STC_CORE_HAS_PORT5 != 1)) || \
    ((STC_CORE_HAS_PORT6 != 0) && (STC_CORE_HAS_PORT6 != 1)) || \
    ((STC_CORE_HAS_PORT7 != 0) && (STC_CORE_HAS_PORT7 != 1)) || \
    ((STC_CORE_HAS_PORT8 != 0) && (STC_CORE_HAS_PORT8 != 1)) || \
    ((STC_CORE_HAS_PORT9 != 0) && (STC_CORE_HAS_PORT9 != 1)) || \
    ((STC_CORE_HAS_PORTA != 0) && (STC_CORE_HAS_PORTA != 1)) || \
    ((STC_CORE_HAS_PORTB != 0) && (STC_CORE_HAS_PORTB != 1)) || \
    ((STC_CORE_HAS_PORT_MODE != 0) && (STC_CORE_HAS_PORT_MODE != 1)) || \
    ((STC_CORE_TIMER1_IS_1T != 0) && (STC_CORE_TIMER1_IS_1T != 1)) || \
    ((STC_CORE_HAS_UART1 != 0) && (STC_CORE_HAS_UART1 != 1)) || \
    ((STC_CORE_HAS_ADC != 0) && (STC_CORE_HAS_ADC != 1)) || \
    ((STC_CORE_HAS_SEPARATE_PULLUP != 0) && \
     (STC_CORE_HAS_SEPARATE_PULLUP != 1)) || \
    ((STC_CORE_SERIAL_BUFFERED_RX != 0) && \
     (STC_CORE_SERIAL_BUFFERED_RX != 1))
# error "STC core capability flags must be 0 or 1"
#endif

#if STC_CORE_HAS_SEPARATE_PULLUP && !STC_CORE_HAS_PORT_MODE
# error "Separate pull-up control requires configurable port modes"
#endif

#if (STC_CORE_HAS_PORT8 || STC_CORE_HAS_PORT9 || STC_CORE_HAS_PORTA || \
     STC_CORE_HAS_PORTB) && !defined(STC_CORE_FAMILY_32)
# error "Extended P8/P9/PA/PB ports are only supported on STC32 targets"
#endif

#if !STC_CORE_HAS_UART1 && STC_CORE_SERIAL_BUFFERED_RX
# error "Buffered UART receive requires UART1"
#endif

#if !STC_CORE_HAS_ADC
# if (STC_CORE_ADC_LAYOUT != STC_ADC_LAYOUT_NONE) || \
     (STC_CORE_ADC_NATIVE_BITS != 0)
#  error "A target without ADC must select layout NONE and zero native bits"
# endif
#elif STC_CORE_ADC_LAYOUT == STC_ADC_LAYOUT_MODERN_BC_ADCCFG
# if (!defined(STC_CORE_FAMILY_32) && \
      !defined(STC_CORE_FAMILY_AI8051U)) || \
     ((STC_CORE_ADC_NATIVE_BITS != 10) && (STC_CORE_ADC_NATIVE_BITS != 12))
#  error "The modern ADC layout requires a 10/12-bit STC32/AI target"
# endif
#else
# error "Unsupported STC ADC register layout"
#endif

#define STC_CORE_USES_MCS251 1
#define STC_CORE_HAS_TIMER01_16BIT_AUTO_RELOAD 1
#define STC_CORE_HAS_MODERN_UART1_BRT 1

/* The classic INT0/INT1 pins and vectors are common to every target. */
#define STC_CORE_HAS_INT0 1
#define STC_CORE_HAS_INT1 1

/* ITn=0 is BOTH edges on modern STC parts, not classic 8051 LOW level.
 * 0: only FALLING verified; 2: CHANGE/FALLING.
 */
#define STC_CORE_INT01_MODE 2

#endif
