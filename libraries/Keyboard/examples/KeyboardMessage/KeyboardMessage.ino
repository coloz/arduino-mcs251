/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Keyboard.h>

// Connect a button between P3.2 and GND. Open a text editor before pressing it.
const uint8_t buttonPin = P3_2;
bool lastReading = false;
bool stablePressed = false;
unsigned long changedAt = 0;
unsigned long pressCount = 0;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Keyboard.begin();
}

void loop() {
  bool reading = digitalRead(buttonPin) == LOW;
  if (reading != lastReading) {
    lastReading = reading;
    changedAt = millis();
  }

  if (millis() - changedAt >= 25 && reading != stablePressed) {
    stablePressed = reading;
    if (stablePressed && USBDevice.configured()) {
      Keyboard.print("Button press ");
      Keyboard.println(++pressCount);
    }
  }
}
