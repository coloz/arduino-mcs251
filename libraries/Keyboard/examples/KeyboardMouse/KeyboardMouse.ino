/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Keyboard.h>
#include <Mouse.h>

// Connect a button from P3.2 to GND. One action per press.
void setup() {
  pinMode(P3_2, INPUT_PULLUP);
  Keyboard.begin();
  Mouse.begin();
}
void loop() {
  static bool wasPressed;
  bool pressed = digitalRead(P3_2) == LOW;
  if (pressed && !wasPressed && USBDevice.configured()) {
    Keyboard.println("Hello from STC!");
    Mouse.move(10, 0);
  }
  wasPressed = pressed;
  delay(20);
}
