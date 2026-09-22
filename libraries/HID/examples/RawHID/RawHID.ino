/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   AI8051U-34K16, AI8051U-34K32, AI8051U-34K64.
 */

#include <HID.h>
// Vendor-defined report ID 3, eight-byte input and output reports.
const uint8_t descriptor[] PROGMEM = {
  0x06,0x00,0xff,0x09,0x01,0xa1,0x01,0x85,0x03,
  0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x08,
  0x09,0x01,0x81,0x02,0x95,0x08,0x09,0x01,0x91,0x02,0xc0
};
HIDSubDescriptor node(descriptor, sizeof(descriptor));
void setup() {
  HID().AppendDescriptor(&node);
  HID().RegisterReport(3, 8);
  HID().begin();
}
void loop() {
  uint8_t packet[64];
  int count = HID().read(packet, sizeof(packet));
  if (count == 9 && packet[0] == 3) HID().SendReport(3, packet + 1, 8);
}
