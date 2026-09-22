#ifndef STC_CORE_WIRING_DIGITAL_PRIVATE_H
#define STC_CORE_WIRING_DIGITAL_PRIVATE_H

#include <stdint.h>

/*
 * Tracks Arduino input-mode semantics independently from each STC port latch.
 * It lives in a tiny archive member so fixed-function peripherals can update
 * their pin ownership without pulling the complete digital-I/O implementation.
 */
extern uint8_t __stc_digital_input_pins[];

#endif
