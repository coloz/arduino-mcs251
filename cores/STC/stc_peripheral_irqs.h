#ifndef STC_PERIPHERAL_IRQS_H
#define STC_PERIPHERAL_IRQS_H
#include "Arduino.h"
typedef void (*stc_peripheral_service_t)(void) STC_REENTRANT;
#if defined(__SDCC)
#define STC_IRQ_DATA __data
#else
#define STC_IRQ_DATA
#endif
extern STC_IRQ_DATA stc_peripheral_service_t stc_tone_service;
extern STC_IRQ_DATA stc_peripheral_service_t stc_wire_slave_service;
typedef void (*stc_uart_service_t)(uint8_t port) STC_REENTRANT;
extern STC_IRQ_DATA stc_uart_service_t stc_uart_extra_service;
#if STC_CORE_I2C_COUNT > 1
extern STC_IRQ_DATA stc_peripheral_service_t stc_wire1_slave_service;
#endif
#endif
