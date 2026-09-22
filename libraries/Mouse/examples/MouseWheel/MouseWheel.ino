/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Mouse.h>

// Buttons to GND: P3.2 scrolls up, P3.3 scrolls down, P3.4 is middle click.
const uint8_t upPin = P3_2;
const uint8_t downPin = P3_3;
const uint8_t middlePin = P3_4;

void setup() {
  pinMode(upPin, INPUT_PULLUP);
  pinMode(downPin, INPUT_PULLUP);
  pinMode(middlePin, INPUT_PULLUP);
  Mouse.begin();
}

void loop() {
  if (USBDevice.configured()) {
    if (digitalRead(middlePin) == LOW) Mouse.press(MOUSE_MIDDLE);
    else Mouse.release(MOUSE_MIDDLE);
    signed char wheel = (digitalRead(upPin) == LOW) - (digitalRead(downPin) == LOW);
    if (wheel) Mouse.move(0, 0, wheel);
  } else {
    Mouse.release(MOUSE_ALL);
  }
  delay(100);  // Holding a scroll button produces ten steps per second.
}
