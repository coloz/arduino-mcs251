/* Small CDC echo for all native-USB variants, including AI8051U-34K16.
 * Select USB CDC On Boot > Enabled and open the new USB serial port.
 */
#if !ARDUINO_USB_CDC_ON_BOOT
#error Select USB CDC On Boot > Enabled for this example
#endif
void setup() {
  Serial.begin(115200);
}
void loop() {
  if (Serial.available()) Serial.write((uint8_t)Serial.read());
}
