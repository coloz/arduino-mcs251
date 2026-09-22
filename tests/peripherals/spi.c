/* Run the production SPI engines against register and GPIO boundaries.
 * Link SPI.c, SPI1.c, SPI2.c and SPITransactions.c as separate objects. */
#include "Arduino.h"
#include "SPI_backend.h"

#if STC_CORE_SPI_COUNT > 1
#define DECLARE_BUS(prefix) \
    void prefix##begin(void); \
    void prefix##end(void); \
    uint8_t prefix##configurationError(void); \
    uint8_t prefix##setPinsChecked(uint8_t,uint8_t,uint8_t,uint8_t); \
    void prefix##setSettings(unsigned long,uint8_t,uint8_t); \
    uint8_t prefix##beginTransactionChecked(unsigned long,uint8_t,uint8_t); \
    void prefix##endTransaction(void); \
    uint8_t prefix##transfer(uint8_t)
DECLARE_BUS(SPI1_);
DECLARE_BUS(SPI2_);
#undef DECLARE_BUS
#endif

static uint8_t regs[3][6], initial_config[3], initial_prescale[3], initial_clock[3];
static uint8_t clock_regs[7]; /* CLKSEL, CLKDIV, PLL1CR/PDIV, PLL2CR/PDIV, HSCLKDIV */
static uint8_t mux_sfr, mux_xfr, gate, interrupt_state;
static uint8_t invalid_access, closed_gate_access, timeout_bus = 255u, collision_bus = 255u;
static uint8_t levels[256], modes[256], last_sent[3];
static unsigned int register_writes[3], data_writes[3], gpio_writes;
static uint8_t watched_clock, watched_mosi, software_input, software_lsb;
static uint8_t software_reads, software_sent, clock_writes, data_pin_writes;
static unsigned long ticks;

uint8_t stc_spi_host_interrupt_state_read(void) { return interrupt_state; }
void stc_spi_host_interrupt_state_write(uint8_t value) { interrupt_state = value; }
uint8_t stc_spi_hw_gate_read(void) { return gate; }
void stc_spi_hw_gate_write(uint8_t value) { gate = value; }

uint8_t stc_spi_hw_read(uint8_t bus, uint8_t reg)
{
    if (bus >= STC_CORE_SPI_COUNT || reg >= 13u) { invalid_access = 1; return 0; }
    if ((bus || reg >= 3u) && !(gate & 0x80u)) closed_gate_access = 1;
    if (reg >= 6u) return clock_regs[reg - 6u];
    return regs[bus][reg];
}
void stc_spi_hw_write(uint8_t bus, uint8_t reg, uint8_t value)
{
    if (bus >= STC_CORE_SPI_COUNT || reg >= 6u) { invalid_access = 1; return; }
    if ((bus || reg >= 3u) && !(gate & 0x80u)) closed_gate_access = 1;
    ++register_writes[bus];
    if (reg == 1u) {
        regs[bus][1] &= (uint8_t)~value; /* SPIF/WCOL are write-one-to-clear. */
    } else if (reg == 2u) {
        ++data_writes[bus]; last_sent[bus] = value;
        regs[bus][2] = value ^ (uint8_t)(0x5au + bus);
        if (bus != timeout_bus) regs[bus][1] |= 0x80u;
        if (bus == collision_bus) regs[bus][1] |= 0x40u;
    } else regs[bus][reg] = value;
}
uint8_t stc_spi_hw_mux_read(uint8_t bus)
{
    if (bus >= STC_CORE_SPI_COUNT) { invalid_access = 1; return 0; }
    if (bus && !(gate & 0x80u)) closed_gate_access = 1;
    return bus ? mux_xfr : mux_sfr;
}
void stc_spi_hw_mux_write(uint8_t bus, uint8_t value)
{
    if (bus >= STC_CORE_SPI_COUNT) { invalid_access = 1; return; }
    if (bus && !(gate & 0x80u)) closed_gate_access = 1;
    if (bus) mux_xfr = value; else mux_sfr = value;
}
void pinMode(uint8_t pin, uint8_t mode) { ++gpio_writes; modes[pin] = mode; }
void digitalWrite(uint8_t pin, uint8_t value)
{
    ++gpio_writes; levels[pin] = value;
    if (pin == watched_clock) ++clock_writes;
    if (pin == watched_mosi) ++data_pin_writes;
}
int digitalRead(uint8_t pin)
{
    uint8_t mask = software_lsb ? (uint8_t)(1u << software_reads) :
                                 (uint8_t)(0x80u >> software_reads);
    (void)pin;
    if (levels[watched_mosi]) software_sent |= mask;
    ++software_reads;
    return (software_input & mask) ? HIGH : LOW;
}
int digitalPinIsValid(uint8_t pin)
{
    static const uint8_t masks[12] = {
        PIN_VALID_MASK_P0, PIN_VALID_MASK_P1, PIN_VALID_MASK_P2, PIN_VALID_MASK_P3,
        PIN_VALID_MASK_P4, PIN_VALID_MASK_P5, PIN_VALID_MASK_P6, PIN_VALID_MASK_P7,
        PIN_VALID_MASK_P8, PIN_VALID_MASK_P9, PIN_VALID_MASK_PA, PIN_VALID_MASK_PB
    };
    return (pin >> 4) < 12u && (pin & 15u) < 8u &&
           (masks[pin >> 4] & (1u << (pin & 7u)));
}
void delayMicroseconds(unsigned int delay) { ticks += delay; }
unsigned long micros(void) { return ++ticks; }

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
int run_tests(void)
{
    uint8_t bus, route, mode, order, speed, before, bytes[2];
    unsigned int writes, gpios;
    mux_sfr = 0xa6u; mux_xfr = 0xf0u; gate = 0x31u; interrupt_state = 0xa5u;
    watched_clock = watched_mosi = 255u;
    clock_regs[2] = 0x80u; clock_regs[3] = 2u; /* 12 MHz BASE / 2 -> 6 MHz PLL reference; HSIO=156 MHz. */
    for (bus = 0; bus < STC_CORE_SPI_COUNT; ++bus) {
        regs[bus][3] = initial_config[bus] = 0xd5u;
        regs[bus][4] = initial_prescale[bus] = 0x91u + bus;
        regs[bus][5] = initial_clock[bus] = 0x61u + bus;
#if STC_CORE_SPI_LAYOUT == 3
        regs[bus][5] = initial_clock[bus] = 0; /* AI SPI clock divider reset/bypass. */
#endif
    }

    CHECK(SPI_setPinsChecked(255u,P3_3,P3_2,P3_5) == STC_SPI_INVALID);
    CHECK(SPI_setPinsChecked(P3_4,P3_4,P3_2,P3_5) == STC_SPI_INVALID);
#if STC_VARIANT_PIN_ALIAS_GROUP_COUNT > 0
# if defined(STC32CL8K64) || defined(STC32CL8K48)
    CHECK(SPI_setPinsChecked(P1_4,P0_2,P3_2,P3_5) == STC_SPI_INVALID);
# elif defined(STC_CORE_FAMILY_AI8051U)
    CHECK(SPI_setPinsChecked(P4_4,P4_5,P3_2,P3_5) == STC_SPI_INVALID);
# elif defined(STC32G144K246)
    CHECK(SPI_setPinsChecked(P1_3,P1_7,P3_2,P3_5) == STC_SPI_INVALID);
# endif
#endif
    CHECK(register_writes[0] == 0u && gpio_writes == 0u);
    SPI_begin(); /* The default 100 kHz deliberately uses GPIO. */
    CHECK(SPI_configurationError() == STC_SPI_OK && regs[0][0] == 0u);
    CHECK(modes[PIN_SPI_MISO] == INPUT && modes[PIN_SPI_MOSI] == OUTPUT);
    CHECK(levels[PIN_SPI_SS] == HIGH);
    CHECK(SPI_beginTransactionChecked(0,MSBFIRST,SPI_MODE0) == STC_SPI_INVALID);
    CHECK(interrupt_state == 0xa5u);
    /* The legacy PIN_SPI defaults are intentionally preserved. Select an
     * explicit hardware group before checking the new controller backend. */
    CHECK(SPI_setPinsChecked(PIN_SPI1_MOSI,PIN_SPI1_MISO,PIN_SPI1_SCK,PIN_SPI1_SS) == STC_SPI_OK);

    /* Exercise all modes, both bit orders and each supported divider. */
    for (mode = 0; mode < 4u; ++mode) {
        for (order = 0; order < 2u; ++order) {
            SPI_setSettings(F_CPU / 2UL, order, mode);
            CHECK(!SPI_configurationError());
            CHECK(regs[0][0] == (uint8_t)(0xd3u | (order == LSBFIRST ? 0x20u : 0u) | (mode << 2)));
            CHECK((regs[0][3] & 0x30u) == 0x20u); /* HSSPIEN, no FIFO. */
#if STC_CORE_SPI_LAYOUT != 1
            CHECK(!(regs[0][3] & 0x40u) && regs[0][4] == 0u); /* No MOSI/MISO swap. */
#endif
#if STC_CORE_SPI_LAYOUT == 2
            CHECK(regs[0][5] == 13u);
#endif
            CHECK(SPI_transfer(0xa6u) == (0xa6u ^ 0x5au));
            CHECK(last_sent[0] == 0xa6u && regs[0][1] == 0u);
            CHECK(gate == 0x31u && interrupt_state == 0xa5u);
        }
    }
    for (speed = 0; speed < 3u; ++speed) {
        SPI_setSettings(F_CPU / (4UL << speed),MSBFIRST,SPI_MODE0);
#if STC_CORE_SPI_LAYOUT == 2
        CHECK((regs[0][0] & 3u) == 3u && (regs[0][3] & 0x30u) == 0x20u);
        CHECK(regs[0][5] == (26u << speed));
#else
        CHECK((regs[0][0] & 3u) == speed && !(regs[0][3] & 0x30u));
#endif
    }
    SPI_setSettings(F_CPU / 3UL,MSBFIRST,SPI_MODE0);
#if STC_CORE_SPI_LAYOUT == 2
    CHECK((regs[0][0] & 3u) == 3u && regs[0][5] == 20u); /* 156MHz/2/20=3.9MHz <= 4MHz. */
    CHECK(SPI_usingHardware());
    clock_regs[2] = 0; SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0);
    CHECK(!SPI_usingHardware() && regs[0][0] == 0); /* Selected PLL off. */
    clock_regs[2] = 0x80; clock_regs[3] = 3;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware()); /* Invalid reference. */
    clock_regs[3] = 2; clock_regs[0] = 4;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware()); /* CPU source cannot establish BASECLK. */
    clock_regs[0] = 0;
    clock_regs[2] = 0x90; clock_regs[3] = 8;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware() && regs[0][5] == 13); /* IRC48/8. */
    clock_regs[2] = 0x80; clock_regs[3] = 2; clock_regs[4] = 0x40;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware() && regs[0][5] == 26); /* Full PLL1. */
    SPI_setSettings(0xffffffffUL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware() && regs[0][5] == 2); /* Input <=240 MHz, no overflow. */
    clock_regs[2] = 0x8f;
    SPI_setSettings(0xffffffffUL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware() && regs[0][5] == 3); /* 492MHz source. */
    clock_regs[2] = 0x80;
    clock_regs[4] = 0xa0; clock_regs[5] = 2;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware() && regs[0][5] == 13); /* PLL2/2. */
    clock_regs[4] = 0; clock_regs[5] = 0;
    SPI_setSettings(1,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware()); /* Divider beyond 8 bits. */
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware());
#else
    CHECK((regs[0][0] & 3u) == 0u); /* Requested rate must not be exceeded. */
    clock_regs[0]=0x40; SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware());
    clock_regs[0]=0; clock_regs[1]=2;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware());
    clock_regs[1]=0; clock_regs[6]=2;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware());
    clock_regs[6]=0;
#if STC_CORE_SPI_LAYOUT == 3
    regs[0][5]=2; SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(!SPI_usingHardware());
    regs[0][5]=0;
#endif
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0); CHECK(SPI_usingHardware());
#endif
    for (route = 0; route < 4u; ++route) {
        uint8_t mosi = STC_VARIANT_SPI1_MOSI_PIN(route);
        if (mosi == 255u) continue; /* Unbonded route on the selected variant. */
        CHECK(SPI_setPinsChecked(mosi,STC_VARIANT_SPI1_MISO_PIN(route),
              STC_VARIANT_SPI1_SCK_PIN(route),P3_5) == STC_SPI_OK);
        CHECK((mux_sfr & 0x0cu) == (uint8_t)(route << 2));
        CHECK((mux_sfr & 0xf3u) == (0xa6u & 0xf3u));
    }

    /* Custom pins always use GPIO, even at a hardware-compatible rate. */
    CHECK(SPI_setPinsChecked(P3_4,P3_1,P3_2,P3_5) == STC_SPI_OK);
    CHECK(regs[0][0] == 0u && regs[0][3] == initial_config[0] && mux_sfr == 0xa6u);
    watched_clock = P3_2; watched_mosi = P3_4; writes = data_writes[0];
    for (mode = 0; mode < 4u; ++mode) {
        for (order = 0; order < 2u; ++order) {
            SPI_setSettings(100000UL,order,mode);
            software_input = 0x96u; software_lsb = order == LSBFIRST;
            software_reads = software_sent = clock_writes = data_pin_writes = 0u;
            CHECK(SPI_transfer(0xa6u) == 0x96u && software_sent == 0xa6u);
            CHECK(software_reads == 8u && clock_writes == 17u && data_pin_writes == 8u);
            CHECK(levels[P3_2] == ((mode & 2u) ? HIGH : LOW));
            CHECK(data_writes[0] == writes);
        }
    }
    watched_clock = watched_mosi = 255u;
    CHECK(SPI_setPinsChecked(PIN_SPI1_MOSI,PIN_SPI1_MISO,PIN_SPI1_SCK,PIN_SPI1_SS) == STC_SPI_OK);
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0);
    bytes[0] = 0x12u; bytes[1] = 0x34u; SPI_transferBuffer(bytes,2u);
    CHECK(bytes[0] == (0x12u ^ 0x5au) && bytes[1] == (0x34u ^ 0x5au));
    writes = data_writes[0]; SPI_transferBuffer(0,2u); CHECK(data_writes[0] == writes);
    collision_bus = 0u;
    CHECK(SPI_transfer(1u) == 255u && SPI_configurationError() == STC_SPI_BUSY);
    CHECK(gate == 0x31u && regs[0][1] == 0u); collision_bus = 255u;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0);
    timeout_bus = 0u;
    CHECK(SPI_transfer(1u) == 255u && SPI_configurationError() == STC_SPI_TIMEOUT);
    CHECK(regs[0][0] == 0u && mux_sfr == 0xa6u && regs[0][3] == initial_config[0]);
    CHECK(gate == 0x31u && interrupt_state == 0xa5u); timeout_bus = 255u;
    SPI_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE0);

    SPI_usingInterrupt(0u);
    CHECK(SPI_beginTransactionChecked(F_CPU / 4UL,MSBFIRST,SPI_MODE1) == STC_SPI_OK);
    CHECK(interrupt_state == 0xa4u);
    CHECK(SPI_beginTransactionChecked(F_CPU / 2UL,MSBFIRST,SPI_MODE0) == STC_SPI_BUSY);
    interrupt_state ^= 2u; /* Ending must preserve unrelated interrupt changes. */
    SPI_endTransaction(); CHECK(interrupt_state == 0xa7u); SPI_notUsingInterrupt(0u);
    SPI_usingInterrupt(255u);
    CHECK(SPI_beginTransactionChecked(F_CPU / 2UL,MSBFIRST,SPI_MODE0) == STC_SPI_OK);
    CHECK(interrupt_state == 0x27u);
    SPI_endTransaction(); CHECK(interrupt_state == 0xa7u);

#if STC_CORE_SPI_COUNT > 1
    SPI1_begin(); SPI2_begin();
    CHECK(SPI1_setPinsChecked(P0_3,P0_4,P0_5,P0_2) == STC_SPI_OK);
    CHECK(SPI2_setPinsChecked(P8_5,P8_6,P8_7,P8_4) == STC_SPI_OK);
    SPI1_setSettings(F_CPU / 4UL,LSBFIRST,SPI_MODE1);
    SPI2_setSettings(F_CPU / 8UL,MSBFIRST,SPI_MODE2);
    CHECK(regs[0][0] == 0xd3u && regs[1][0] == 0xf7u && regs[2][0] == 0xdbu);
    CHECK(mux_xfr == 0xf9u && (mux_sfr & 0x0cu) == 0u);
    CHECK((regs[1][3] & 0x70u) == 0x20u && (regs[2][3] & 0x70u) == 0x20u);
    CHECK(regs[1][5] == 26u && regs[2][5] == 52u);
    CHECK(SPI1_transfer(0x11u) == (0x11u ^ 0x5bu));
    CHECK(SPI2_transfer(0x22u) == (0x22u ^ 0x5cu));

    CHECK(SPI_beginTransactionChecked(F_CPU / 2UL,MSBFIRST,SPI_MODE0) == STC_SPI_OK);
    CHECK(interrupt_state == 0x27u);
    writes = register_writes[1]; gpios = gpio_writes; before = regs[1][0];
    CHECK(SPI1_beginTransactionChecked(F_CPU / 2UL,MSBFIRST,SPI_MODE3) == STC_SPI_BUSY);
    CHECK(SPI1_transfer(0u) == 255u && SPI1_configurationError() == STC_SPI_BUSY);
    CHECK(SPI1_setPinsChecked(P6_5,P6_6,P6_7,P6_4) == STC_SPI_BUSY);
    SPI1_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE3); CHECK(SPI1_configurationError() == STC_SPI_BUSY);
    SPI1_begin(); SPI1_end(); SPI1_endTransaction();
    CHECK(register_writes[1] == writes && gpio_writes == gpios && regs[1][0] == before);
    CHECK(interrupt_state == 0x27u && mux_xfr == 0xf9u);
    SPI_endTransaction(); CHECK(interrupt_state == 0xa7u);
    SPI1_setSettings(F_CPU / 2UL,MSBFIRST,SPI_MODE3);
    CHECK(regs[1][0] == 0xdfu && (regs[1][3] & 0x70u) == 0x20u);
    CHECK(regs[2][0] == 0xdbu && (regs[2][3] & 0x70u) == 0x20u);
    SPI1_end();
    CHECK(regs[1][0] == 0u && regs[2][0] == 0xdbu && mux_xfr == 0xf8u);
    CHECK(regs[1][3] == initial_config[1] && regs[1][4] == initial_prescale[1] && regs[1][5] == initial_clock[1]);
    SPI2_end(); CHECK(mux_xfr == 0xf0u && regs[2][3] == initial_config[2]);
#else
    (void)before; (void)gpios;
#endif
    SPI_notUsingInterrupt(255u);
    SPI_end();
    CHECK(regs[0][0] == 0u && regs[0][3] == initial_config[0] && mux_sfr == 0xa6u);
#if STC_CORE_SPI_LAYOUT != 1
    CHECK(regs[0][4] == initial_prescale[0]);
#endif
#if STC_CORE_SPI_LAYOUT == 2
    CHECK(regs[0][5] == initial_clock[0]);
#endif
    CHECK(gate == 0x31u && interrupt_state == 0xa7u && !invalid_access && !closed_gate_access);
    return 0;
}
