/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// CAN: RX=P0.0, TX=P0.1. CAN1: RX=P0.2, TX=P0.3.
// Use two transceivers on the same terminated CAN bus.
// CAN sends a counter; CAN1 receives it and supplies the bus ACK.
bool canReady = false;
unsigned long lastSend = 0;
uint8_t counter = 0;

void setup() {
  Serial.begin(115200);
  CAN.setPins(P0_0, P0_1);
  CAN1.setPins(P0_2, P0_3);
  bool firstReady = CAN.begin(CanBitRate::BR_500k);
  bool secondReady = CAN1.begin(CanBitRate::BR_500k);
  canReady = firstReady && secondReady;
  if (!canReady) {
    CAN.end();
    CAN1.end();
    Serial.println("Dual CAN initialization failed");
  }
}

void loop() {
  if (!canReady) return;

  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    CanMsg message(CanStandardId(0x321), 1, &counter);
    int result = CAN.write(message);
    if (result == 1) ++counter;
    else {
      Serial.print("CAN write error: ");
      Serial.println(result);
    }
  }

  if (CAN1.available()) {
    Serial.print("CAN1 received: ");
    Serial.println(CAN1.read());
  }
  // Drain any other traffic received by the first controller.
  if (CAN.available()) (void)CAN.read();
}
