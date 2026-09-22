#include <SPI.h>

// SPI object numbers are zero-based; PIN_SPI<n> macros use hardware numbers.
// For a loopback test connect each bus's MOSI to its own MISO. CS is GPIO.
volatile uint8_t replies[STC_CORE_SPI_COUNT];
volatile uint8_t errors[STC_CORE_SPI_COUNT];
volatile bool hardware[STC_CORE_SPI_COUNT];

void exchange(SPIClass &bus, uint8_t index, uint8_t select)
{
    errors[index] = bus.beginTransactionChecked(SPISettings(F_CPU / 2UL, MSBFIRST, SPI_MODE0));
    if (errors[index]) return;
    hardware[index] = bus.usingHardware();
    digitalWrite(select, LOW);
    replies[index] = bus.transfer(0xa0u + index);
    digitalWrite(select, HIGH);
    errors[index] = bus.configurationError();
    bus.endTransaction();
}

void setup()
{
    errors[0] = SPI.setPinsChecked(PIN_SPI1_MOSI, PIN_SPI1_MISO, PIN_SPI1_SCK, PIN_SPI1_SS);
    SPI.begin();
#if STC_CORE_SPI_COUNT > 1
    SPI1.begin(); // SPI2: P6.5/P6.6/P6.7, select P6.4
    SPI2.begin(); // SPI3: P2.3/P2.4/P2.5, select P2.2
#endif
}

void loop()
{
    exchange(SPI, 0, PIN_SPI1_SS);
#if STC_CORE_SPI_COUNT > 1
    exchange(SPI1, 1, PIN_SPI2_SS);
    exchange(SPI2, 2, PIN_SPI3_SS);
#endif
    delay(100);
}
