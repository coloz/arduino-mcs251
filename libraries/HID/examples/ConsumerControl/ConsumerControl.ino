/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K32, AI8051U-34K64.
 */

#include <HID.h>

// Buttons to GND: play/pause=P3.2, volume up=P3.3,
// volume down=P3.4, mute=P3.5.
const uint8_t buttonPins[] = {P3_2, P3_3, P3_4, P3_5};
const uint16_t usages[] = {0x00CD, 0x00E9, 0x00EA, 0x00E2};
const uint8_t reportId = 3;
const uint8_t reportDescriptor[] PROGMEM = {
  0x05,0x0C, 0x09,0x01, 0xA1,0x01, 0x85,reportId, // Consumer Control
  0x15,0x00, 0x26,0xFF,0x03,                       // Logical range 0..1023
  0x19,0x00, 0x2A,0xFF,0x03,                       // Usage range 0..1023
  0x75,0x10, 0x95,0x01, 0x81,0x00, 0xC0           // One 16-bit array item
};
HIDSubDescriptor node(reportDescriptor, sizeof(reportDescriptor));
bool usbReady = false;
uint16_t lastReading = 0;
uint16_t stableUsage = 0;
uint16_t lastSent = 0;
unsigned long changedAt = 0;
unsigned long lastReportAt = 0;

void setup() {
  for (uint8_t i = 0; i < 4; ++i) pinMode(buttonPins[i], INPUT_PULLUP);
  HID().AppendDescriptor(&node);
  usbReady = node.registered && HID().RegisterReport(reportId, 2) && HID().begin();
}

void loop() {
  uint16_t reading = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    if (digitalRead(buttonPins[i]) == LOW) {
      reading = usages[i];  // First pressed button wins.
      break;
    }
  }
  unsigned long now = millis();
  if (reading != lastReading) {
    lastReading = reading;
    changedAt = now;
  }
  if (now - changedAt >= 25) stableUsage = reading;

  if (usbReady && HID().configured() &&
      (stableUsage != lastSent || now - lastReportAt >= 100)) {
    // Zero releases the key. Serialize the 16-bit usage little-endian.
    uint8_t report[] = {(uint8_t)stableUsage, (uint8_t)(stableUsage >> 8)};
    if (HID().SendReport(reportId, report, sizeof(report)) > 0) {
      lastSent = stableUsage;
      lastReportAt = now;
    }
  }
}
