/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Mouse.h>
void setup() {
  pinMode(P3_2, INPUT_PULLUP);
  Mouse.begin();
}
void loop() {
  if (USBDevice.configured()) {
    if (digitalRead(P3_2) == LOW) Mouse.press(MOUSE_LEFT);
    else Mouse.release(MOUSE_LEFT);
  }
  delay(10);
}
