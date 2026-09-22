
#include "SPIClass.h"
#include <hal/stc_c_hal.h>

#if STC_CORE_SPI_COUNT > 1
#define SPI_CALL(name, ...) (_bus == 1u ? SPI1_##name(__VA_ARGS__) : \
                            _bus == 2u ? SPI2_##name(__VA_ARGS__) : SPI_##name(__VA_ARGS__))
#else
#define SPI_CALL(name, ...) SPI_##name(__VA_ARGS__)
#endif

SPIClass SPI;
#if STC_CORE_SPI_COUNT > 1
SPIClass SPI1(1u);
SPIClass SPI2(2u);
#endif

bool SPIClass::usingHardware() const
{
    return _bus < STC_CORE_SPI_COUNT && SPI_CALL(usingHardware) != 0u;
}

void SPIClass::begin()
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(begin);
}

void SPIClass::end()
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(end);
}

void SPIClass::setPins(uint8_t mosi, uint8_t miso, uint8_t clock,
                       uint8_t select)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(setPins, mosi, miso, clock, select);
}

void SPIClass::beginTransaction(const SPISettings &settings)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    (void)beginTransactionChecked(settings);
}

uint8_t SPIClass::configurationError() { return _bus < STC_CORE_SPI_COUNT ? SPI_CALL(configurationError) : STC_SPI_INVALID; }
uint8_t SPIClass::setPinsChecked(uint8_t mosi, uint8_t miso, uint8_t clock, uint8_t select)
{
    if (_bus >= STC_CORE_SPI_COUNT) return STC_SPI_INVALID;
    return SPI_CALL(setPinsChecked, mosi, miso, clock, select);
}
uint8_t SPIClass::beginTransactionChecked(const SPISettings &settings)
{
    if (_bus >= STC_CORE_SPI_COUNT) return STC_SPI_INVALID;
    uint8_t status = SPI_CALL(beginTransactionChecked, (unsigned long)settings._clock,
                                                 settings._bitOrder, settings._dataMode);
    if (status != 0u) return status;
    _clock = settings._clock;
    _bitOrder = settings._bitOrder;
    _dataMode = settings._dataMode;
    return status;
}

void SPIClass::beginTransaction(uint32_t clock, uint8_t bitOrder,
                                uint8_t dataMode)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    beginTransaction(SPISettings(clock, bitOrder, dataMode));
}

void SPIClass::endTransaction()
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(endTransaction);
}

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(usingInterrupt, interruptNumber);
}

void SPIClass::notUsingInterrupt(uint8_t interruptNumber)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(notUsingInterrupt, interruptNumber);
}

uint8_t SPIClass::transfer(uint8_t value)
{
    if (_bus >= STC_CORE_SPI_COUNT) return 0xffu;
    return SPI_CALL(transfer, value);
}

uint16_t SPIClass::transfer16(uint16_t value)
{
    if (_bus >= STC_CORE_SPI_COUNT) return 0xffffu;
    uint8_t first;
    uint8_t second;

    if (_bitOrder == LSBFIRST) {
        first = SPI_CALL(transfer, (uint8_t)value);
        second = SPI_CALL(transfer, (uint8_t)(value >> 8));
        return (uint16_t)((uint16_t)first | ((uint16_t)second << 8));
    }

    first = SPI_CALL(transfer, (uint8_t)(value >> 8));
    second = SPI_CALL(transfer, (uint8_t)value);
    return (uint16_t)(((uint16_t)first << 8) | (uint16_t)second);
}

void SPIClass::transfer(void *buffer, size_t length)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(transferBuffer, static_cast<uint8_t *>(buffer), length);
}

void SPIClass::setBitOrder(uint8_t bitOrder)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(setSettings, (unsigned long)_clock, bitOrder,
                    _dataMode);
    if (SPI_CALL(configurationError) == 0u) _bitOrder = bitOrder;
}

void SPIClass::setDataMode(uint8_t dataMode)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    SPI_CALL(setSettings, (unsigned long)_clock, _bitOrder,
                    dataMode);
    if (SPI_CALL(configurationError) == 0u) _dataMode = dataMode;
}

void SPIClass::setClockDivider(uint8_t clockDivider)
{
    if (_bus >= STC_CORE_SPI_COUNT) return;
    uint32_t requestedClock;
#if defined(F_CPU)
    /* Spell out the ratios so the freestanding frontend can fold every
     * division at compile time; no target integer-division helper is needed. */
    if (clockDivider == SPI_CLOCK_DIV2) {
        requestedClock = (uint32_t)(F_CPU / 2UL);
    } else if (clockDivider == SPI_CLOCK_DIV4) {
        requestedClock = (uint32_t)(F_CPU / 4UL);
    } else if (clockDivider == SPI_CLOCK_DIV8) {
        requestedClock = (uint32_t)(F_CPU / 8UL);
    } else if (clockDivider == SPI_CLOCK_DIV16) {
        requestedClock = (uint32_t)(F_CPU / 16UL);
    } else if (clockDivider == SPI_CLOCK_DIV32) {
        requestedClock = (uint32_t)(F_CPU / 32UL);
    } else if (clockDivider == SPI_CLOCK_DIV64) {
        requestedClock = (uint32_t)(F_CPU / 64UL);
    } else if (clockDivider == SPI_CLOCK_DIV128) {
        requestedClock = (uint32_t)(F_CPU / 128UL);
    } else {
        requestedClock = (uint32_t)SPI_DEFAULT_CLOCK_HZ;
    }
    if (requestedClock == 0u) {
        requestedClock = 1u;
    }
#else
    /* Host-only consumers without a board definition retain the safe HAL
     * default instead of inventing a CPU frequency. */
    (void)clockDivider;
    requestedClock = (uint32_t)SPI_DEFAULT_CLOCK_HZ;
#endif
    SPI_CALL(setSettings, (unsigned long)requestedClock, _bitOrder,
                    _dataMode);
    if (SPI_CALL(configurationError) == STC_SPI_OK) _clock = requestedClock;
}
