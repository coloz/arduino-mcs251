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
#endif
