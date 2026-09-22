/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Mouse.h>

// Joystick axes: A0/P1.0 and A1/P1.1, centered near half the ADC range.
// Hold P3.2 to GND to enable movement; P3.3 to GND holds the left button.
const uint8_t enablePin = P3_2;
const uint8_t clickPin = P3_3;

int readAxis(uint8_t pin) {
  int centered = analogRead(pin) - 512;
  if (centered > -64 && centered < 64) return 0;
  return centered / 64;
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(enablePin, INPUT_PULLUP);
  pinMode(clickPin, INPUT_PULLUP);
  analogReadResolution(10);
  Mouse.begin();
}

void loop() {
  if (USBDevice.configured() && digitalRead(enablePin) == LOW) {
    int x = readAxis(A0);
    int y = readAxis(A1);
    if (digitalRead(clickPin) == LOW) Mouse.press(MOUSE_LEFT);
    else Mouse.release(MOUSE_LEFT);
    if (x || y) Mouse.move(x, y);
  } else {
    Mouse.release(MOUSE_ALL);
  }
  delay(10);
}
