# Wire

On the six supported 64 KiB+ targets, eight frequently accessed scalar bytes
use DATA. RX/TX buffer sizes and callback behavior are unchanged. Use the
platform-wide `-DSTC_WIRE_STATE_IN_DATA=0` to retain XDATA placement when
application DATA is scarce.

`Wire.begin()` selects master mode. The existing hardware/software master,
repeated START, transaction status and clock-stretch timeout APIs remain available.
`Wire.begin(address)`, `onReceive(void (*)(int))`, and
`onRequest(void (*)(void))` enable a hardware I2C slave (`WIRE_HAS_SLAVE=1`).

Slave mode uses SDA=P3.3 and SCL=P3.2 with external pull-ups. When no pins
have been selected, `begin(address)` selects this route automatically. An
explicit incompatible `setPins` configuration is rejected. Check
`Wire.configurationError()` after initialization. Addresses must be 1–127;
general-call address zero is not enabled. Master and slave operation are
alternative modes of the same controller, not simultaneous roles.

Callbacks execute in interrupt context. `onReceive` reads the received data
using `available`/`read`; `onRequest` supplies a reply using `write`. Keep
callbacks short; do not call blocking Serial, SD, delay or master Wire
transactions inside them. Copy shared application data with appropriate
interrupt protection. A repeated START delivers the previous write before
the read callback. Default receive/transmit buffers hold 32 bytes; excess
receive data is NACKed and reply underruns return 0xff. `end()` disables the
controller interrupt, releases both pins to input mode and restores the mux.

See [SlaveEcho](examples/SlaveEcho/SlaveEcho.ino). The host state-machine tests
exercise address/data, repeated START, ACK/NACK, overflow and shutdown; target
builds cover all ten variants. Physical bus timing and interaction with other
active peripherals still require board-level validation.
