/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Keyboard.h>

// Connect active-HIGH LEDs through resistors from these pins to GND.
const uint8_t numLockLed = P2_0;
const uint8_t capsLockLed = P2_1;
const uint8_t scrollLockLed = P2_2;

void setup() {
  pinMode(numLockLed, OUTPUT);
  pinMode(capsLockLed, OUTPUT);
  pinMode(scrollLockLed, OUTPUT);
  Keyboard.begin();
}

void loop() {
  // Read the host's output report, not a locally tracked key state.
  uint8_t leds = USBDevice.configured() ? Keyboard.leds() : 0;
  digitalWrite(numLockLed, (leds & 0x01) ? HIGH : LOW);
  digitalWrite(capsLockLed, (leds & 0x02) ? HIGH : LOW);
  digitalWrite(scrollLockLed, (leds & 0x04) ? HIGH : LOW);
  delay(10);
}
