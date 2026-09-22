/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Mouse.h>

// Connect each button between its pin and GND.
const uint8_t upPin = P3_2;
const uint8_t downPin = P3_3;
const uint8_t leftPin = P3_4;
const uint8_t rightPin = P3_5;
const uint8_t clickPin = P3_6;
const int step = 5;

void setup() {
  pinMode(upPin, INPUT_PULLUP);
  pinMode(downPin, INPUT_PULLUP);
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(rightPin, INPUT_PULLUP);
  pinMode(clickPin, INPUT_PULLUP);
  Mouse.begin();
}

void loop() {
  if (USBDevice.configured()) {
    int x = (digitalRead(rightPin) == LOW) - (digitalRead(leftPin) == LOW);
    int y = (digitalRead(downPin) == LOW) - (digitalRead(upPin) == LOW);
    if (digitalRead(clickPin) == LOW) Mouse.press(MOUSE_LEFT);
    else Mouse.release(MOUSE_LEFT);
    if (x || y) Mouse.move(x * step, y * step);
  } else {
    Mouse.release(MOUSE_ALL);
  }
  delay(10);
}
