#include "Arduino.h"
#if STC_CORE_SPI_COUNT > 1
/* Zero means idle; otherwise the owner is the zero-based bus index + 1. */
volatile uint8_t stc_spi_transaction_owner;
#endif
