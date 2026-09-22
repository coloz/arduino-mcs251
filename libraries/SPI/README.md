# SPI

All supported chips have one independent hardware SPI controller;
STC32G144K246 has three. The objects are `SPI` (hardware SPI1), `SPI1`
(hardware SPI2) and `SPI2` (hardware SPI3); the last two exist only on G144.
USART synchronous SPI modes and QSPI are separate peripherals and are not
implemented by this library.

The public Arduino SPI API retains transaction interrupt guards, configurable
pins, supported hardware routes and the software fallback. Chip selects remain
under application control; devices sharing a bus need distinct selects.

The existing `SPI` default wiring is preserved: STC32G12/G8/CL uses the P1
hardware route; AI8051U and G144 retain P3.2/P3.3/P3.4/P3.5 software wiring.
Select a complete hardware route explicitly when needed:

```cpp
SPI.setPinsChecked(PIN_SPI1_MOSI, PIN_SPI1_MISO, PIN_SPI1_SCK, PIN_SPI1_SS);
SPI.begin();
SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
digitalWrite(PIN_SPI1_SS, LOW);
uint8_t reply = SPI.transfer(0x9f);
digitalWrite(PIN_SPI1_SS, HIGH);
SPI.endTransaction();
```

The `PIN_SPI1_*`, `PIN_SPI2_*`, `PIN_SPI3_*` macros use the hardware controller
number. Default MOSI/MISO/SCK are P1.3/P1.4/P1.5 on STC32 SPI1 and
P1.5/P1.6/P1.7 on AI8051U SPI; G144 SPI2 defaults to P6.5/P6.6/P6.7 and SPI3
to P2.3/P2.4/P2.5. SS is an application-controlled GPIO and need not be the
controller's dedicated slave-select pin. Full bonded route tables and counts
are generated in each variant; custom GPIO or requests below the hardware's
available clock range retain software SPI. Hardware rates never exceed the
requested rate; software timing also includes GPIO overhead.

G144 uses an independent HSIO clock, which must not be assumed equal to
`F_CPU`. The driver reads the selected enabled PLL and validates its 6 MHz
reference (BASECLK with a provable CPU divider, or IRC48M divided by 8), then
sets the SPI controller's divider without changing shared PLL/HSIO settings.
If the input clock cannot be established, the requested divider is out of
range, or the PLL is disabled, it uses GPIO SPI. Call `usingHardware()` after
`beginTransactionChecked()` to inspect the active backend. In particular,
selecting G144 hardware pins alone does not initialize its PLL.

On classic STC32 and AI8051U, hardware SPI is selected only while the CPU
and high-speed clock dividers/source confirm the core's `F_CPU` clock model.
Application changes to these shared clocks outside that model cause a GPIO
fallback instead of an incorrectly clocked transfer.

See [HardwareBuses](examples/HardwareBuses/HardwareBuses.ino) for a single- or
three-bus loopback example that records this flag. AI8051U-34K16 has limited
Flash: the SPI-only example fits, while examples combining Serial formatting
and SPI may exceed 16 KB; inspect the build report for the actual application.

G144 buses keep separate settings and state, but transactions must finish
before starting a transaction on another bus. Overlapping transactions are
rejected with `STC_SPI_BUSY` to preserve registered interrupt guards. Use
`beginTransactionChecked()` / `configurationError()` to check errors.
Only master mode is implemented. Transfers use bounded polling; a timeout or
write collision is reported instead of blocking indefinitely. `attachInterrupt`
and `detachInterrupt` remain compatibility no-ops: no SPI completion callback
is exposed.

Resource coverage and remaining driver gaps are listed in
[HARDWARE_INTERFACES.md](../../variants/HARDWARE_INTERFACES.md). Register and mux sources:
[STC32G manual](https://www.stcmicro.com/datasheet/stc32g-cn.pdf),
[G144 manual, chapter 25](https://www.stcmicro.com/datasheet/STC32G144K246-cn.pdf),
[AI8051U pin tables](https://www.stcmicro.com/datasheet/Ai8051U_Features.pdf).

On STC32G12K128, STC32G144K246, AI8051U-34K64, STC32G12K64, STC32G8K64 and
STC32CL8K64, eight bytes of frequently accessed bus state use DATA to reduce
Flash spent accessing XDATA. Transaction bookkeeping keeps its original
placement. To revert, pass `-DSTC_SPI_STATE_IN_DATA=0` through platform-wide
`build.extra_flags`; defining it only in a sketch cannot configure SPI.c.
