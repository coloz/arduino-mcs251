/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// Connect a second CAN node at 500 kbit/s. Standard and extended IDs
// are distinct even when their numeric values happen to match.
bool canReady = false;
unsigned long lastSend = 0;
uint8_t sequence = 0;

void setup() {
  Serial.begin(115200);
  canReady = CAN.begin(CanBitRate::BR_500k);
  if (!canReady) Serial.println("CAN initialization failed");
}

void loop() {
  if (!canReady) return;

  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    const uint8_t payload[] = {sequence, 0x51, 0x25};
    CanMsg message(CanExtendedId(0x18FF5010UL), sizeof(payload), payload);
    int result = CAN.write(message);
    if (result == 1) ++sequence;
    else {
      Serial.print("CAN write error: ");
      Serial.println(result);
    }
  }

  if (CAN.available()) {
    CanMsg message = CAN.read();
    Serial.print(message.isExtendedId() ? "Extended " : "Standard ");
    Serial.println(message);
  }
}
