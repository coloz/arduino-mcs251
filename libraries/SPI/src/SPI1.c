/* Hardware controller 2, only present on STC32G144 variants. */
#include "Arduino.h"
#if STC_CORE_SPI_COUNT > 1
#define STC_SPI_INSTANCE 1
#define STC_SPI_STATE_IN_DATA 0
#define SPI_DEFAULT_MOSI_PIN PIN_SPI2_MOSI
#define SPI_DEFAULT_MISO_PIN PIN_SPI2_MISO
#define SPI_DEFAULT_SCK_PIN PIN_SPI2_SCK
#define SPI_DEFAULT_SS_PIN PIN_SPI2_SS
#include "SPI1_aliases.h"
#include "SPIInstance.h"
#endif
