/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <Keyboard.h>

// UART1: connect adapter TX to P3.0, RX to P3.1, and share GND.
// Send ASCII at 115200 baud while holding the P3.2-to-GND enable button.
// The chip's native USB connection supplies the keyboard interface.
const uint8_t enablePin = P3_2;

void setup() {
  pinMode(enablePin, INPUT_PULLUP);
  Serial.begin(115200);
  Keyboard.begin();
}

void loop() {
  if (!Serial.available()) return;
  int incoming = Serial.read();
  // Consume disabled input so it will not be typed later.
  if (digitalRead(enablePin) != LOW || !USBDevice.configured()) return;

  if (incoming == '\n') Keyboard.write(KEY_RETURN);
  else if (incoming == '\t') Keyboard.write(KEY_TAB);
  else if (incoming == '\b') Keyboard.write(KEY_BACKSPACE);
  else if (incoming >= 32 && incoming <= 126) Keyboard.write((uint8_t)incoming);
  // Ignore CR, allowing a CRLF-terminated line to produce a single Enter.
}
