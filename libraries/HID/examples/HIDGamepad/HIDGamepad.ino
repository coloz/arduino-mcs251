/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <HID.h>

// Joystick axes: A0/P1.0 and A1/P1.1. Four buttons connect P3.2..P3.5 to GND.
const uint8_t buttonPins[] = {P3_2, P3_3, P3_4, P3_5};
const uint8_t reportId = 4;
const uint8_t reportDescriptor[] PROGMEM = {
  0x05,0x01, 0x09,0x05, 0xA1,0x01, 0x85,reportId, // Game Pad
  0x05,0x09, 0x19,0x01, 0x29,0x04,
  0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x04,
  0x81,0x02,                                     // Four buttons
  0x75,0x04, 0x95,0x01, 0x81,0x03,               // Four padding bits
  0x05,0x01, 0x09,0x30, 0x09,0x31,
  0x15,0x81, 0x25,0x7F, 0x75,0x08, 0x95,0x02,
  0x81,0x02, 0xC0                                // Signed absolute X/Y
};
HIDSubDescriptor node(reportDescriptor, sizeof(reportDescriptor));
bool usbReady = false;
uint8_t lastButtons = 0;
uint8_t stableButtons = 0;
unsigned long changedAt = 0;

uint8_t readAxis(uint8_t pin) {
  int value = (analogRead(pin) >> 2) - 128;
  if (value < -127) value = -127;
  return (uint8_t)value;  // Signed HID axis encoded as one byte.
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  analogReadResolution(10);
  for (uint8_t i = 0; i < 4; ++i) pinMode(buttonPins[i], INPUT_PULLUP);
  HID().AppendDescriptor(&node);
  usbReady = node.registered && HID().RegisterReport(reportId, 3) && HID().begin();
}

void loop() {
  uint8_t buttons = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    if (digitalRead(buttonPins[i]) == LOW) buttons |= (uint8_t)(1u << i);
  }
  if (buttons != lastButtons) {
    lastButtons = buttons;
    changedAt = millis();
  }
  if (millis() - changedAt >= 25) stableButtons = buttons;

  if (usbReady && HID().configured()) {
    // Build the bytes explicitly; no compiler-dependent struct packing.
    uint8_t report[] = {stableButtons, readAxis(A0), readAxis(A1)};
    HID().SendReport(reportId, report, sizeof(report));
  }
  delay(10);
}
