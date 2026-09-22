# AI8051U LED / UART / SSD1306 hardware check

This example targets the Seekfree AI8051U-34K64 core board configured for
MCS251 execution and a 40 MHz system clock. Build with:

```text
stc:mcs251:ai8051u_34k64:clock=40m
```

The clock menu does not configure the physical oscillator or execution mode.
For simultaneous display/status output and 64-byte UART echo requests, pass
`--build-property build.extra_flags=-DSERIAL_RX_BUFFER_SIZE=128` to Arduino CLI.
The core defaults to a 16-byte RX ring (15 usable); a burst arriving while a
status line is being sent can overflow it. The build property must apply to
the entire core, not just a `#define` in this sketch. `RX_OVF` counts detected
receive overflows; such partial command lines are rejected.
P5.2 is the board's active-low LED: 100 ms on, 100 ms off (5 Hz).
UART1 uses P3.0/RX and P3.1/TX, exposed through the board's USB serial bridge,
at **115200 baud, 8N1**.

Connect an IIC SSD1306 module with power disconnected:

| Module | Core board |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | P3.2 |
| SCL / SCK | P3.3 |

Both IIC signals require pull-ups to 3V3; use the module's existing pull-ups
or add one 4.7 kohm resistor per signal. This example uses the core's software
`Wire` master, not the AI8051U hardware IIC controller. The requested clock is
100 kHz; GPIO and function-call overhead reduce the actual bus frequency.

No IIC transfer occurs at startup. Send commands ending in a newline:

| Command | Result |
| --- | --- |
| `E hello` | `ECHO hello` (up to 77 payload characters) |
| `S` | Report elapsed milliseconds, LED toggles, pin level and IIC counters |
| `I` | Probe only 7-bit addresses 0x3C and 0x3D |
| `O64` | Initialize a 128 x 64 SSD1306 and continuously draw text/animation |
| `O32` | Initialize a 128 x 32 SSD1306 and continuously draw text/animation |
| `X` | Stop display updates; the last image remains visible |

Expected display: `AI8051U IIC OK`, a changing frame number, and moving bars.
LED and UART continue during rendering. Wire transactions fit its default
32-byte buffer; screen data is sent in 16-byte chunks. A 25 ms clock-stretch
timeout and error handling prevent a held-low bus from blocking forever.

Status code 0 means ACK, 2 means address NACK, 4 means another bus error,
and 5 means timeout. SSD1306 has no readable controller identity through this
interface: address ACK alone does not prove that the connected part is SSD1306.
Verify the visible text, frame counter and animation as well as the ACKs.

The initialization and page-addressed writes follow the
[Solomon Systech SSD1306 datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf),
sections 8.1.5 and 10.1.3. This is a small diagnostic renderer, without a
full-frame RAM allocation or an external graphics library.

Example flashing command, after exporting the HEX:

```powershell
stc-cli flash --port COM31 --expect AI8051U_34K64 --file AI8051UHardware.ino.hex --execution-mode mcs251 --allow-experimental --force-unverified-target --reset none --wait 8
```

The named board's STC USB bridge supported automatic ISP entry in local
testing. For a different bridge, use its supported reset method. The public
UART protocol checks bootloader acknowledgements and checksums; it does not
provide an independent Flash readback or a documented device-ID check.
