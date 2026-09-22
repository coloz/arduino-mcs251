/* Explicit USB serial also works with USB CDC On Boot disabled.
 * SerialUSB is an alias of USBSerial. No USB-to-UART adapter is needed.
 */
#include <USB.h>
void setup() {
  USBSerial.begin(115200);
  USB.begin(); // Optional: USBSerial.begin() also attaches the device.
}
void loop() {
  uint8_t data[64];
  int count = 0;
  while (USBSerial.available() && count < (int)sizeof(data))
    data[count++] = (uint8_t)USBSerial.read();
  if (count) USBSerial.write(data, count);
}
