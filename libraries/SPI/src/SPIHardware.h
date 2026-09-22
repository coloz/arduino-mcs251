/* SPI1 is in SFR space; G144 SPI2/3 use separate XFR register banks.
 * STC32G manual chapters 23/24; G144 chapter 25; AI8051U chapter 25/26.
 */
#if STC_SPI_INSTANCE == 1
#define SPI_HW_BASE 0x7ef800UL
#define SPI_HW_MUX_MASK 0x03u
#define SPI_HW_MUX_SHIFT 0u
#define SPI_HW_ROUTE(mosi,miso,sck) STC_VARIANT_SPI2_ROUTE(mosi,miso,sck)
#elif STC_SPI_INSTANCE == 2
#define SPI_HW_BASE 0x7ef820UL
#define SPI_HW_MUX_MASK 0x0cu
#define SPI_HW_MUX_SHIFT 2u
#define SPI_HW_ROUTE(mosi,miso,sck) STC_VARIANT_SPI3_ROUTE(mosi,miso,sck)
#else
#define SPI_HW_MUX_MASK 0x0cu
#define SPI_HW_MUX_SHIFT 2u
#define SPI_HW_ROUTE(mosi,miso,sck) STC_VARIANT_SPI1_ROUTE(mosi,miso,sck)
#endif

/* Register-hook IDs are shared by all three buses. */
#define SPI_HW_CTRL 0u
#define SPI_HW_STAT 1u
#define SPI_HW_DATA 2u
#define SPI_HW_HSCFG 3u
#define SPI_HW_PSCR 4u
#define SPI_HW_CLKDIV 5u
#define SPI_HW_CLKSEL 6u
#define SPI_HW_CPU_DIV 7u
#define SPI_HW_PLL1CR 8u
#define SPI_HW_PLL1DIV 9u
#define SPI_HW_PLL2CR 10u
#define SPI_HW_PLL2DIV 11u
#define SPI_HW_HS_DIV 12u

#if STC_CORE_SPI_LAYOUT && defined(__SDCC_mcs251)
#define STC_SPI_HARDWARE 1
#if STC_SPI_INSTANCE == 0
static __sfr __at (0xcd) stc_spi_stat;
static __sfr __at (0xce) stc_spi_ctrl;
static __sfr __at (0xcf) stc_spi_data;
#define SPI_HW_CONTROL(v) (stc_spi_ctrl = (v))
#define SPI_HW_STATUS() stc_spi_stat
#define SPI_HW_CLEAR() (stc_spi_stat = 0xc0u)
#define SPI_HW_SEND(v) (stc_spi_data = (v))
#define SPI_HW_RECEIVE() stc_spi_data
#define SPI_HW_MUX_READ() P_SW1
#define SPI_HW_MUX_WRITE(v) (P_SW1 = (v))
#define SPI_HW_HSCFG_READ() STC_XFR8(0x7efbf9UL)
#define SPI_HW_HSCFG_WRITE(v) (STC_XFR8(0x7efbf9UL) = (v))
#define SPI_HW_PSCR_READ() STC_XFR8(0x7efbfbUL)
#define SPI_HW_PSCR_WRITE(v) (STC_XFR8(0x7efbfbUL) = (v))
#define SPI_HW_CLKDIV_READ() STC_XFR8(0x7efe90UL)
#define SPI_HW_CLKDIV_WRITE(v) (STC_XFR8(0x7efe90UL) = (v))
#else
#define SPI_HW_CONTROL(v) (STC_XFR8(SPI_HW_BASE) = (v))
#define SPI_HW_STATUS() STC_XFR8(SPI_HW_BASE + 1u)
#define SPI_HW_CLEAR() (STC_XFR8(SPI_HW_BASE + 1u) = 0xc0u)
#define SPI_HW_SEND(v) (STC_XFR8(SPI_HW_BASE + 2u) = (v))
#define SPI_HW_RECEIVE() STC_XFR8(SPI_HW_BASE + 2u)
#define SPI_HW_MUX_READ() STC_XFR8(0x7efd6cUL)
#define SPI_HW_MUX_WRITE(v) (STC_XFR8(0x7efd6cUL) = (v))
#define SPI_HW_HSCFG_READ() STC_XFR8(SPI_HW_BASE + 10u)
#define SPI_HW_HSCFG_WRITE(v) (STC_XFR8(SPI_HW_BASE + 10u) = (v))
#define SPI_HW_PSCR_READ() STC_XFR8(SPI_HW_BASE + 12u)
#define SPI_HW_PSCR_WRITE(v) (STC_XFR8(SPI_HW_BASE + 12u) = (v))
#define SPI_HW_CLKDIV_READ() STC_XFR8(SPI_HW_BASE + 8u)
#define SPI_HW_CLKDIV_WRITE(v) (STC_XFR8(SPI_HW_BASE + 8u) = (v))
#endif
#define SPI_HW_GATE_READ() P_SW2
#define SPI_HW_GATE_WRITE(v) (P_SW2 = (v))
#define SPI_HW_CLOCK_READ(r) STC_XFR8((r) == SPI_HW_CLKSEL ? 0x7efe00UL : \
    (r) == SPI_HW_CPU_DIV ? 0x7efe01UL : (r) == SPI_HW_HS_DIV ? 0x7efe0bUL : \
    0x7efe0cUL + ((r) - SPI_HW_PLL1CR))
#elif defined(STC_SPI_HOST_HARDWARE_HOOKS)
#define STC_SPI_HARDWARE 1
uint8_t stc_spi_hw_read(uint8_t bus, uint8_t reg);
void stc_spi_hw_write(uint8_t bus, uint8_t reg, uint8_t value);
uint8_t stc_spi_hw_mux_read(uint8_t bus);
void stc_spi_hw_mux_write(uint8_t bus, uint8_t value);
uint8_t stc_spi_hw_gate_read(void);
void stc_spi_hw_gate_write(uint8_t value);
#define SPI_HW_READ(r) stc_spi_hw_read(STC_SPI_INSTANCE, (r))
#define SPI_HW_WRITE(r,v) stc_spi_hw_write(STC_SPI_INSTANCE, (r), (v))
#define SPI_HW_CONTROL(v) SPI_HW_WRITE(SPI_HW_CTRL, (v))
#define SPI_HW_STATUS() SPI_HW_READ(SPI_HW_STAT)
#define SPI_HW_CLEAR() SPI_HW_WRITE(SPI_HW_STAT, 0xc0u)
#define SPI_HW_SEND(v) SPI_HW_WRITE(SPI_HW_DATA, (v))
#define SPI_HW_RECEIVE() SPI_HW_READ(SPI_HW_DATA)
#define SPI_HW_MUX_READ() stc_spi_hw_mux_read(STC_SPI_INSTANCE)
#define SPI_HW_MUX_WRITE(v) stc_spi_hw_mux_write(STC_SPI_INSTANCE, (v))
#define SPI_HW_HSCFG_READ() SPI_HW_READ(SPI_HW_HSCFG)
#define SPI_HW_HSCFG_WRITE(v) SPI_HW_WRITE(SPI_HW_HSCFG, (v))
#define SPI_HW_PSCR_READ() SPI_HW_READ(SPI_HW_PSCR)
#define SPI_HW_PSCR_WRITE(v) SPI_HW_WRITE(SPI_HW_PSCR, (v))
#define SPI_HW_CLKDIV_READ() SPI_HW_READ(SPI_HW_CLKDIV)
#define SPI_HW_CLKDIV_WRITE(v) SPI_HW_WRITE(SPI_HW_CLKDIV, (v))
#define SPI_HW_GATE_READ() stc_spi_hw_gate_read()
#define SPI_HW_GATE_WRITE(v) stc_spi_hw_gate_write(v)
#define SPI_HW_CLOCK_READ(r) SPI_HW_READ(r)
#endif

#ifdef STC_SPI_HARDWARE
static uint8_t spi_hardware, spi_saved_mux, spi_saved_hscfg;
#if STC_CORE_SPI_LAYOUT != 1
static uint8_t spi_saved_pscr;
#endif
#if STC_CORE_SPI_LAYOUT == 2
static uint8_t spi_saved_clkdiv;
#endif
static unsigned long spi_clock_hz = SPI_DEFAULT_CLOCK_HZ;
static uint8_t spi_hw_enter(void)
{
    uint8_t saved = SPI_HW_GATE_READ() & 0x80u;
    SPI_HW_GATE_WRITE(SPI_HW_GATE_READ() | 0x80u);
    return saved;
}
static void spi_hw_leave(uint8_t saved)
{
    SPI_HW_GATE_WRITE((SPI_HW_GATE_READ() & 0x7fu) | saved);
}
#if STC_CORE_SPI_LAYOUT == 2
/* G144 SPI is clocked by HSIO, not by SYSCLK. Do not reprogram the shared
 * PLL: PWM, I2S and TFPU can already be using it. Only accept a PLL whose
 * documented 6 MHz reference can be verified from the selected input. */
static unsigned long spi_hsio_clock(void)
{
    uint8_t select = (SPI_HW_CLOCK_READ(SPI_HW_PLL2CR) >> 5) & 3u;
    uint8_t control = SPI_HW_CLOCK_READ((select & 1u) ? SPI_HW_PLL2CR : SPI_HW_PLL1CR);
    uint8_t divider = SPI_HW_CLOCK_READ((select & 1u) ? SPI_HW_PLL2DIV : SPI_HW_PLL1DIV);
    uint8_t cpu_div;
    unsigned long clock;
    if (!(control & 0x80u) || !divider) return 0UL;
    if (control & 0x10u) {
        if (divider != 8u) return 0UL; /* Dedicated 48 MHz IRC / 8. */
    } else {
        if (SPI_HW_CLOCK_READ(SPI_HW_CLKSEL) & 0x0cu) return 0UL;
        cpu_div = SPI_HW_CLOCK_READ(SPI_HW_CPU_DIV);
        if (!cpu_div) cpu_div = 1u;
        if (F_CPU > 4294967295UL / cpu_div ||
            F_CPU * cpu_div != 6000000UL * divider) return 0UL;
    }
    clock = 6000000UL * (52u + 2u * (control & 15u));
    return select < 2u ? clock / 2UL : clock;
}
#endif
static void spi_hardware_disable(void)
{
    uint8_t saved_ea, gate;
    if (!spi_hardware) return;
    saved_ea = spi_lock_registration();
    gate = spi_hw_enter();
    SPI_HW_CONTROL(0u);
    SPI_HW_MUX_WRITE((SPI_HW_MUX_READ() & (uint8_t)~SPI_HW_MUX_MASK) | spi_saved_mux);
    SPI_HW_HSCFG_WRITE(spi_saved_hscfg);
#if STC_CORE_SPI_LAYOUT != 1
    SPI_HW_PSCR_WRITE(spi_saved_pscr);
#endif
#if STC_CORE_SPI_LAYOUT == 2
    SPI_HW_CLKDIV_WRITE(spi_saved_clkdiv);
#endif
    spi_hardware = 0u;
    spi_hw_leave(gate);
    spi_unlock_registration(saved_ea);
}
static void spi_hardware_configure(void)
{
    uint8_t route, speed, saved_ea, gate, config;
#if STC_CORE_SPI_LAYOUT == 2
    unsigned long source, divider;
#endif
    route = SPI_HW_ROUTE(spi_mosi_pin, spi_miso_pin, spi_sck_pin);
    /* Below the lowest audited hardware rate, preserve a GPIO fallback. */
    if (route == 255u) {
        spi_hardware_disable();
        return;
    }
#if STC_CORE_SPI_LAYOUT == 2
    gate = spi_hw_enter();
    source = spi_hsio_clock();
    spi_hw_leave(gate);
    if (!source) { spi_hardware_disable(); return; }
    divider = spi_clock_hz >= source / 2UL ? 1UL :
        (source + 2UL * spi_clock_hz - 1UL) / (2UL * spi_clock_hz);
    /* The G144 SPI input clock is specified up to 240 MHz, even when
     * another HSIO consumer selected an undivided 312--492 MHz PLL. */
    if (source > 480000000UL && divider < 3UL) divider = 3UL;
    else if (source > 240000000UL && divider < 2UL) divider = 2UL;
    if (divider > 255UL) { spi_hardware_disable(); return; }
    speed = 3u;
#else
    if (spi_clock_hz < F_CPU / 16UL) { spi_hardware_disable(); return; }
    /* With unmodified MCLK/HSIO dividers both the ordinary and high-speed
     * clock paths equal F_CPU. If an application chose a different shared
     * clock, retain GPIO instead of silently generating a faster SCLK. */
    gate = spi_hw_enter();
    config = (SPI_HW_CLOCK_READ(SPI_HW_CLKSEL) & 0x40u) ||
             SPI_HW_CLOCK_READ(SPI_HW_CPU_DIV) > 1u ||
             SPI_HW_CLOCK_READ(SPI_HW_HS_DIV) > 1u;
#if STC_CORE_SPI_LAYOUT == 3
    /* AI8051U has another SPI-only divider following HSCLKDIV. */
    config |= SPI_HW_CLKDIV_READ() > 1u;
#endif
    spi_hw_leave(gate);
    if (config) { spi_hardware_disable(); return; }
    speed = spi_clock_hz >= F_CPU / 2UL ? 3u :
        spi_clock_hz >= F_CPU / 4UL ? 0u : spi_clock_hz >= F_CPU / 8UL ? 1u : 2u;
#endif
    saved_ea = spi_lock_registration();
    gate = spi_hw_enter();
    if (!spi_hardware) {
        spi_saved_mux = SPI_HW_MUX_READ() & SPI_HW_MUX_MASK;
        spi_saved_hscfg = SPI_HW_HSCFG_READ();
#if STC_CORE_SPI_LAYOUT != 1
        spi_saved_pscr = SPI_HW_PSCR_READ();
#endif
#if STC_CORE_SPI_LAYOUT == 2
        spi_saved_clkdiv = SPI_HW_CLKDIV_READ();
#endif
    }
    SPI_HW_CONTROL(0u);
    SPI_HW_MUX_WRITE((SPI_HW_MUX_READ() & (uint8_t)~SPI_HW_MUX_MASK) |
                     (uint8_t)(route << SPI_HW_MUX_SHIFT));
    /* F_CPU/2 requires HSSPIEN. FIFO suppresses SPIF and is DMA-only. */
    config = SPI_HW_HSCFG_READ() & (uint8_t)~0x30u;
#if STC_CORE_SPI_LAYOUT != 1
    config &= (uint8_t)~0x40u; /* Normal MOSI/MISO ordering. */
    SPI_HW_PSCR_WRITE(0u);
#endif
#if STC_CORE_SPI_LAYOUT == 2
    SPI_HW_CLKDIV_WRITE((uint8_t)divider);
#endif
    SPI_HW_HSCFG_WRITE(config | (speed == 3u ? 0x20u : 0u));
    SPI_HW_CLEAR();
    SPI_HW_CONTROL(0xd0u | (spi_bit_order == LSBFIRST ? 0x20u : 0u) |
                   (spi_data_mode << 2) | speed);
    spi_hardware = 1u;
    spi_hw_leave(gate);
    spi_unlock_registration(saved_ea);
}
static uint8_t spi_hardware_transfer(uint8_t value)
{
    uint16_t budget = 1024u;
    uint8_t received, gate = spi_hw_enter();
    SPI_HW_CLEAR();
    SPI_HW_SEND(value);
    while (!(SPI_HW_STATUS() & 0x80u)) {
        if (--budget == 0u) {
            spi_config_error = STC_SPI_TIMEOUT;
            spi_hardware_disable();
            spi_hw_leave(gate);
            return 0xffu;
        }
    }
    if (SPI_HW_STATUS() & 0x40u) {
        spi_config_error = STC_SPI_BUSY;
        SPI_HW_CLEAR();
        spi_hw_leave(gate);
        return 0xffu;
    }
    received = SPI_HW_RECEIVE();
    SPI_HW_CLEAR();
    spi_hw_leave(gate);
    return received;
}
#endif
