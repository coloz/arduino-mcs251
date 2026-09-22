/* Native USB: STC32G12K64/128, STC32G144K246, AI8051U-34K16/32/64.
 * Select Tools > USB CDC On Boot > Enabled. Open the new USB COM port.
 */
#if !ARDUINO_USB_CDC_ON_BOOT
#error Select USB CDC On Boot > Enabled for this example
#endif
void setup() {
  pinMode(P5_2, OUTPUT);
  Serial.begin(115200);
  // Wait briefly for the monitor; still run when no computer is connected.
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(1);
}
void loop() {
  digitalWrite(P5_2, HIGH);
  delay(2000);
  digitalWrite(P5_2, LOW);
  delay(2000);
  Serial.println("Hello from USB CDC!");
}
