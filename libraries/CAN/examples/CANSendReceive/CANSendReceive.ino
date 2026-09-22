/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

void setup() {
  Serial.begin(115200);
  if (!CAN.begin(CanBitRate::BR_500k)) {
    Serial.print("CAN begin failed: "); Serial.println(CAN.lastError());
  }
}

void loop() {
  static unsigned long lastSend;
  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    const uint8_t bytes[] = {0x51, 0x25, 0x01};
    CanMsg message(CanStandardId(0x123), sizeof(bytes), bytes);
    if (CAN.write(message) < 0) Serial.println("CAN send unavailable");
  }
  while (CAN.available()) Serial.println(CAN.read());
}
