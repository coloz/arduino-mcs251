/* Independent I2C register banks; selector values come from the variant. */
#if STC_WIRE_INSTANCE == 1
#define WIRE_HW_BASE 0x7ef860UL
#define WIRE_HW_MUX_MASK 0xc0u
#define WIRE_HW_MUX_SHIFT 6u
#define WIRE_HW_ROUTE(sda,scl) STC_VARIANT_I2C2_ROUTE(sda,scl)
#define WIRE_HW_DEFAULT_SDA PIN_I2C2_SDA
#define WIRE_HW_DEFAULT_SCL PIN_I2C2_SCL
#else
#define WIRE_HW_BASE 0x7efe80UL
#define WIRE_HW_MUX_MASK 0x30u
#define WIRE_HW_MUX_SHIFT 4u
#define WIRE_HW_ROUTE(sda,scl) STC_VARIANT_I2C1_ROUTE(sda,scl)
#define WIRE_HW_DEFAULT_SDA PIN_I2C1_SDA
#define WIRE_HW_DEFAULT_SCL PIN_I2C1_SCL
#endif
#if defined(__SDCC_mcs251)
#define STC_WIRE_HARDWARE 1
#define WIRE_HW_READ(r) STC_XFR8(WIRE_HW_BASE + (r))
#define WIRE_HW_WRITE(r,v) (STC_XFR8(WIRE_HW_BASE + (r)) = (v))
#define WIRE_HW_GATE_READ() P_SW2
#define WIRE_HW_GATE_WRITE(v) (P_SW2 = (v))
#if STC_WIRE_INSTANCE == 1
#define WIRE_HW_MUX_READ() STC_XFR8(0x7efd6cUL)
#define WIRE_HW_MUX_WRITE(v) (STC_XFR8(0x7efd6cUL) = (v))
#else
#define WIRE_HW_MUX_READ() P_SW2
#define WIRE_HW_MUX_WRITE(v) (P_SW2 = (v))
#endif
#elif defined(STC_WIRE_HOST_HARDWARE_HOOKS)
#define STC_WIRE_HARDWARE 1
uint8_t stc_wire_hw_read(uint8_t reg);
void stc_wire_hw_write(uint8_t reg, uint8_t value);
uint8_t stc_wire_hw_mux_read(void);
void stc_wire_hw_mux_write(uint8_t value);
#define WIRE_HW_READ(r) stc_wire_hw_read(r)
#define WIRE_HW_WRITE(r,v) stc_wire_hw_write(r,v)
#define WIRE_HW_MUX_READ() stc_wire_hw_mux_read()
#define WIRE_HW_MUX_WRITE(v) stc_wire_hw_mux_write(v)
uint8_t stc_wire_hw_gate_read(void);
void stc_wire_hw_gate_write(uint8_t value);
#define WIRE_HW_GATE_READ() stc_wire_hw_gate_read()
#define WIRE_HW_GATE_WRITE(v) stc_wire_hw_gate_write(v)
#endif

#ifdef STC_WIRE_HARDWARE
static uint8_t wire_hardware, wire_saved_mux, wire_hw_status;
static unsigned long wire_clock_hz = WIRE_DEFAULT_CLOCK_HZ;
static void wire_handle_timeout(void);
static uint8_t wire_hw_enter(void)
{
    uint8_t saved = WIRE_HW_GATE_READ() & 0x80u;
    WIRE_HW_GATE_WRITE(WIRE_HW_GATE_READ() | 0x80u);
    return saved;
}
static void wire_hw_leave(uint8_t saved)
{
    WIRE_HW_GATE_WRITE((WIRE_HW_GATE_READ() & 0x7fu) | saved);
}
static void wire_hardware_end(void)
{
    uint8_t saved;
    if (!wire_hardware) return;
    saved = wire_hw_enter(); WIRE_HW_WRITE(0u, 0u);
    WIRE_HW_MUX_WRITE((WIRE_HW_MUX_READ() & (uint8_t)~WIRE_HW_MUX_MASK) | wire_saved_mux);
    wire_hw_leave(saved);
    wire_hardware = 0u;
}
static void wire_hardware_begin(void)
{
#if STC_CORE_WIRE_LAYOUT == 0
    /* Preserve the existing software-master selection on AI8051U. Its
     * independent hardware slave uses the common I2CSL register block. */
    wire_hardware_end();
#else
    unsigned long divider;
    uint8_t saved, route;
    wire_hardware_end();
    route = WIRE_HW_ROUTE(wire_sda_pin, wire_scl_pin);
    if (route == 255u) return;
    divider = wire_clock_hz >= F_CPU / 8UL ? 0UL :
        (F_CPU + 4UL * wire_clock_hz - 1UL) / (4UL * wire_clock_hz) - 2UL;
#if STC_CORE_WIRE_LAYOUT == 2
    if (divider > 16383UL) return;
#else
    if (divider > 63UL) return; /* Software preserves requested lower speeds. */
#endif
    saved = wire_hw_enter();
    wire_saved_mux = WIRE_HW_MUX_READ() & WIRE_HW_MUX_MASK;
    WIRE_HW_MUX_WRITE((WIRE_HW_MUX_READ() & (uint8_t)~WIRE_HW_MUX_MASK) |
                      (uint8_t)(route << WIRE_HW_MUX_SHIFT));
    WIRE_HW_WRITE(0u, 0u); WIRE_HW_WRITE(8u, 0u); /* Manual commands, no DMA. */
#if STC_CORE_WIRE_LAYOUT == 2
    /* STC32G144 manual 27.1.4: I2CPSCR holds divider bits 13:6.
     * Always write it, including zero after switching to a faster clock. */
    WIRE_HW_WRITE(9u, (uint8_t)(divider >> 6));
#endif
    WIRE_HW_WRITE(1u, 0u); WIRE_HW_WRITE(2u, 0u);
    WIRE_HW_WRITE(0u, 0xc0u | ((uint8_t)divider & 0x3fu));
    wire_hw_leave(saved); wire_hardware = 1u;
#endif
}
static uint8_t wire_hardware_command(uint8_t command)
{
    uint8_t saved = wire_hw_enter(), polls = 32u;
    unsigned long started;
    WIRE_HW_WRITE(2u, WIRE_HW_READ(2u) & (uint8_t)~0x40u);
    WIRE_HW_WRITE(1u, command);
    /* Normal START/ACK/STOP commands finish without converting micros(). */
    while (!(WIRE_HW_READ(2u) & 0x40u) && --polls) {}
    if (!(WIRE_HW_READ(2u) & 0x40u)) {
        started = micros();
        while (!(WIRE_HW_READ(2u) & 0x40u)) {
            if (wire_stretch_timeout_us && micros() - started >= wire_stretch_timeout_us) {
                wire_hw_leave(saved); wire_handle_timeout(); return 0u;
            }
        }
    }
    wire_hw_status = WIRE_HW_READ(2u);
    WIRE_HW_WRITE(2u, wire_hw_status & (uint8_t)~0x40u);
    wire_hw_leave(saved); return 1u;
}
static void wire_hardware_reset(void)
{
    uint8_t saved = wire_hw_enter(), config = WIRE_HW_READ(0u);
    WIRE_HW_WRITE(0u, 0u); WIRE_HW_WRITE(2u, 0u); WIRE_HW_WRITE(0u, config);
    wire_hw_leave(saved);
}
static uint8_t wire_hardware_write(uint8_t value)
{
    uint8_t saved = wire_hw_enter();
    WIRE_HW_WRITE(6u, value); wire_hw_leave(saved);
    if (!wire_hardware_command(2u) || !wire_hardware_command(3u)) return WIRE_INTERNAL_TIMEOUT;
    /* MSACKI (bit 1) is the received ACK; MSACKO (bit 0) is the
     * independently latched ACK/NACK that the master sends on reads. */
    return wire_hw_status & 2u ? WIRE_INTERNAL_NACK : WIRE_INTERNAL_ACK;
}
static uint8_t wire_hardware_read(uint8_t send_ack, uint8_t *value)
{
    uint8_t saved;
    if (!wire_hardware_command(4u)) return 0u;
    saved = wire_hw_enter(); *value = WIRE_HW_READ(7u);
    WIRE_HW_WRITE(2u, send_ack ? 0u : 1u); wire_hw_leave(saved);
    return wire_hardware_command(5u);
}
#endif
