/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <CANPacket.h>
CANPacketClass packets(CAN);

void setup() {
  packets.begin(500000L);
}
void loop() {
  if (packets.parsePacket()) {
    packets.beginPacket(0x321);
    while (packets.available()) packets.write((uint8_t)packets.read());
    packets.endPacket();
  }
}
