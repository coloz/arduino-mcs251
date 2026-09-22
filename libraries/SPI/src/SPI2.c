/* Hardware controller 3, only present on STC32G144 variants. */
#include "Arduino.h"
#if STC_CORE_SPI_COUNT > 2
#define STC_SPI_INSTANCE 2
#define STC_SPI_STATE_IN_DATA 0
#define SPI_DEFAULT_MOSI_PIN PIN_SPI3_MOSI
#define SPI_DEFAULT_MISO_PIN PIN_SPI3_MISO
#define SPI_DEFAULT_SCK_PIN PIN_SPI3_SCK
#define SPI_DEFAULT_SS_PIN PIN_SPI3_SS
#include "SPI2_aliases.h"
#include "SPIInstance.h"
#endif
