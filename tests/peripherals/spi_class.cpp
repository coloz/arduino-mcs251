/* Exercise the real C++ wrapper against a recording C ABI, independently of
 * register-level tests. In particular invalid objects must never reach bus 0. */
#include "SPIClass.h"
#include "hal/stc_c_hal.h"
static unsigned int calls[3], transfers[3];
static unsigned long clocks[3];
static uint8_t orders[3], modes[3], sent[3][2];
#define MOCK_SPI(prefix, bus) \
extern "C" { \
void prefix##begin(void) { ++calls[bus]; } \
void prefix##end(void) { ++calls[bus]; } \
uint8_t prefix##configurationError(void) { ++calls[bus]; return 0; } \
uint8_t prefix##usingHardware(void) { ++calls[bus]; return 1; } \
void prefix##setPins(uint8_t,uint8_t,uint8_t,uint8_t) { ++calls[bus]; } \
uint8_t prefix##setPinsChecked(uint8_t,uint8_t,uint8_t,uint8_t) { ++calls[bus]; return 0; } \
void prefix##setSettings(unsigned long c,uint8_t b,uint8_t m) { ++calls[bus]; clocks[bus]=c; orders[bus]=b; modes[bus]=m; } \
uint8_t prefix##beginTransactionChecked(unsigned long c,uint8_t b,uint8_t m) { prefix##setSettings(c,b,m); return 0; } \
void prefix##beginTransaction(unsigned long c,uint8_t b,uint8_t m) { prefix##setSettings(c,b,m); } \
void prefix##endTransaction(void) { ++calls[bus]; } \
void prefix##usingInterrupt(uint8_t) { ++calls[bus]; } \
void prefix##notUsingInterrupt(uint8_t) { ++calls[bus]; } \
uint8_t prefix##transfer(uint8_t value) { ++calls[bus]; sent[bus][transfers[bus]++ & 1u]=value; return value; } \
void prefix##transferBuffer(uint8_t*,size_t) { ++calls[bus]; } \
}
MOCK_SPI(SPI_,0)
#if STC_CORE_SPI_COUNT > 1
MOCK_SPI(SPI1_,1)
MOCK_SPI(SPI2_,2)
#endif
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
extern "C" int run_tests(void)
{
    SPIClass invalid(99);
    uint8_t byte=0xa5;
    CHECK(invalid.configurationError()==STC_SPI_INVALID);
    CHECK(!invalid.usingHardware());
    invalid.begin(); invalid.end();
    CHECK(invalid.setPinsChecked(0,1,2,3)==STC_SPI_INVALID);
    CHECK(invalid.beginTransactionChecked(SPISettings(1000000,MSBFIRST,SPI_MODE0))==STC_SPI_INVALID);
    invalid.endTransaction(); invalid.usingInterrupt(0); invalid.notUsingInterrupt(0);
    invalid.setBitOrder(LSBFIRST); invalid.setDataMode(SPI_MODE3); invalid.setClockDivider(SPI_CLOCK_DIV4);
    invalid.transfer(&byte,1);
    CHECK(invalid.transfer(0x5a)==0xff);
    CHECK(invalid.transfer16(0x1234)==0xffff);
    CHECK(byte==0xa5 && calls[0]==0 && calls[1]==0 && calls[2]==0);
    CHECK(SPI.beginTransactionChecked(SPISettings(1000000,MSBFIRST,SPI_MODE0))==0);
    CHECK(SPI.usingHardware());
    CHECK(SPI.transfer16(0x1234)==0x1234 && sent[0][0]==0x12 && sent[0][1]==0x34);
#if STC_CORE_SPI_COUNT > 1
    CHECK(SPI1.beginTransactionChecked(SPISettings(2000000,LSBFIRST,SPI_MODE3))==0);
    CHECK(SPI2.beginTransactionChecked(SPISettings(3000000,MSBFIRST,SPI_MODE1))==0);
    SPI1.setDataMode(SPI_MODE2);
    CHECK(SPI1.transfer16(0x1234)==0x1234 && sent[1][0]==0x34 && sent[1][1]==0x12);
    CHECK(clocks[0]==1000000 && clocks[1]==2000000 && clocks[2]==3000000);
    CHECK(orders[0]==MSBFIRST && orders[1]==LSBFIRST && modes[0]==SPI_MODE0 && modes[1]==SPI_MODE2 && modes[2]==SPI_MODE1);
#endif
    return 0;
}
