/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// Connect CAN RX=P0.0 and TX=P0.1 to a CAN transceiver.
// Run CANRead on a second node at the same bit rate.
bool canReady = false;
uint32_t counter = 0;

void setup() {
  Serial.begin(115200);
  canReady = CAN.begin(CanBitRate::BR_500k);
  if (!canReady) Serial.println("CAN initialization failed");
}

void loop() {
  if (canReady) {
    // Encode explicitly: the counter is little-endian on the wire.
    uint8_t payload[4];
    for (uint8_t i = 0; i < 4; ++i) {
      payload[i] = (uint8_t)(counter >> (8 * i));
    }
    CanMsg message(CanStandardId(0x123), sizeof(payload), payload);
    int result = CAN.write(message);
    if (result == 1) {
      Serial.print("Queued counter: ");
      Serial.println((unsigned long)counter++);
    } else {
      Serial.print("CAN write error: ");
      Serial.println(result);
    }
  }
  delay(1000);
}
