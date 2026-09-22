/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// Connect CAN RX=P0.0 and TX=P0.1 to a CAN transceiver.
// Run CANWrite on a second node; open UART1 at 115200 baud.
bool canReady = false;

void setup() {
  Serial.begin(115200);
  canReady = CAN.begin(CanBitRate::BR_500k);
  if (!canReady) Serial.println("CAN initialization failed");
}

void loop() {
  if (canReady && CAN.available()) {
    CanMsg message = CAN.read();
    Serial.println(message);

    if (message.isStandardId() && !message.isRemoteFrame() &&
        message.getStandardId() == 0x123 && message.data_length == 4) {
      uint32_t counter = 0;
      for (uint8_t i = 0; i < 4; ++i) {
        counter |= (uint32_t)message.data[i] << (8 * i);
      }
      Serial.print("Counter: ");
      Serial.println((unsigned long)counter);
    }
  }
}
