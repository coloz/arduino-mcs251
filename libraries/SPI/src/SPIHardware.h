/* STC32G classic SPI register boundary and controller selection. */
#if STC_CORE_BUS_LAYOUT == 1 && defined(__SDCC)
#define STC_SPI_HARDWARE 1
static __sfr __at (0xcd) stc_spi_stat;
static __sfr __at (0xce) stc_spi_ctrl;
static __sfr __at (0xcf) stc_spi_data;
static __sfr __at (0xa2) stc_spi_mux;
#define SPI_HW_STATUS() stc_spi_stat
#define SPI_HW_CLEAR() (stc_spi_stat = 0xc0u)
#define SPI_HW_SEND(v) (stc_spi_data = (v))
#define SPI_HW_RECEIVE() stc_spi_data
#define SPI_HW_CONTROL(v) (stc_spi_ctrl = (v))
#define SPI_HW_MUX_READ() stc_spi_mux
#define SPI_HW_MUX_WRITE(v) (stc_spi_mux = (v))
#elif defined(STC_SPI_HOST_HARDWARE_HOOKS)
#define STC_SPI_HARDWARE 1
uint8_t stc_spi_hw_status(void);
void stc_spi_hw_clear(void);
void stc_spi_hw_send(uint8_t value);
uint8_t stc_spi_hw_receive(void);
void stc_spi_hw_control(uint8_t value);
uint8_t stc_spi_hw_mux_read(void);
void stc_spi_hw_mux_write(uint8_t value);
#define SPI_HW_STATUS() stc_spi_hw_status()
#define SPI_HW_CLEAR() stc_spi_hw_clear()
#define SPI_HW_SEND(v) stc_spi_hw_send(v)
#define SPI_HW_RECEIVE() stc_spi_hw_receive()
#define SPI_HW_CONTROL(v) stc_spi_hw_control(v)
#define SPI_HW_MUX_READ() stc_spi_hw_mux_read()
#define SPI_HW_MUX_WRITE(v) stc_spi_hw_mux_write(v)
#endif

#ifdef STC_SPI_HARDWARE
static uint8_t spi_hardware, spi_saved_mux;
static unsigned long spi_clock_hz = SPI_DEFAULT_CLOCK_HZ;
static void spi_hardware_disable(void)
{
    uint8_t saved_ea;
    if (!spi_hardware) return;
    SPI_HW_CONTROL(0u);
    saved_ea = spi_lock_registration();
    SPI_HW_MUX_WRITE((SPI_HW_MUX_READ() & (uint8_t)~0x0cu) | spi_saved_mux);
    spi_unlock_registration(saved_ea);
    spi_hardware = 0u;
}
static void spi_hardware_configure(void)
{
    uint8_t route = 0xffu, speed, saved_ea;
    if (spi_mosi_pin == P1_3 && spi_miso_pin == P1_4 && spi_sck_pin == P1_5) route = 0u;
    if (spi_mosi_pin == P2_3 && spi_miso_pin == P2_4 && spi_sck_pin == P2_5) route = 1u;
#if defined(STC_CORE_FAMILY_32)
    /* STC32G manual 23.1: SPI_S=10 selects P4, not USART2's P7 route. */
    if (spi_mosi_pin == P4_0 && spi_miso_pin == P4_1 && spi_sck_pin == P4_3) route = 2u;
#else
    if (spi_mosi_pin == P7_5 && spi_miso_pin == P7_6 && spi_sck_pin == P7_7) route = 2u;
#endif
    if (spi_mosi_pin == P3_4 && spi_miso_pin == P3_3 && spi_sck_pin == P3_2) route = 3u;
    /* Never exceed the requested speed. Slower/custom-pin buses use GPIO. */
    if (route == 0xffu || spi_clock_hz < F_CPU / 16UL) { spi_hardware_disable(); return; }
    speed = spi_clock_hz >= F_CPU / 2UL ? 3u :
        spi_clock_hz >= F_CPU / 4UL ? 0u : spi_clock_hz >= F_CPU / 8UL ? 1u : 2u;
    saved_ea = spi_lock_registration();
    if (!spi_hardware) spi_saved_mux = SPI_HW_MUX_READ() & 0x0cu;
    SPI_HW_MUX_WRITE((SPI_HW_MUX_READ() & (uint8_t)~0x0cu) | (route << 2));
    spi_unlock_registration(saved_ea);
    SPI_HW_CONTROL(0u); SPI_HW_CLEAR();
    SPI_HW_CONTROL(0xd0u | (spi_bit_order == LSBFIRST ? 0x20u : 0u) |
                   (spi_data_mode << 2) | speed);
    spi_hardware = 1u;
}
static uint8_t spi_hardware_transfer(uint8_t value)
{
    uint16_t budget = 1024u;
    uint8_t received;
    SPI_HW_CLEAR(); SPI_HW_SEND(value);
    /* The slowest hardware byte takes 128 oscillator clocks. A bounded
     * poll count works even when an Arduino transaction masks interrupts. */
    while (!(SPI_HW_STATUS() & 0x80u)) {
        if (--budget == 0u) {
            spi_config_error = STC_SPI_TIMEOUT;
            spi_hardware_disable();
            return 0xffu;
        }
    }
    if (SPI_HW_STATUS() & 0x40u) {
        spi_config_error = STC_SPI_BUSY; SPI_HW_CLEAR(); return 0xffu;
    }
    received = SPI_HW_RECEIVE(); SPI_HW_CLEAR(); return received;
}
#endif
