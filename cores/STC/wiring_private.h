#ifndef STC_CORE_WIRING_PRIVATE_H
#define STC_CORE_WIRING_PRIVATE_H

/* Compatibility include for Arduino libraries that use wiring_private.h for
 * the core pin and timing declarations. STC peripheral routing is managed by
 * its Wire/SPI/Serial backends; SAM/SAMD pinPeripheral() is not an STC API. */
#include "Arduino.h"

#endif
