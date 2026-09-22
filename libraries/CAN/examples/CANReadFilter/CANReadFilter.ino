/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// Match standard IDs 0x120 through 0x12F, including CANWrite's 0x123.
// Filtering is performed in software after reception.
const bool useExtendedIds = false;
bool canReady = false;

void setup() {
  Serial.begin(115200);
  if (!CAN.begin(CanBitRate::BR_500k)) {
    Serial.println("CAN initialization failed");
    return;
  }

  if (useExtendedIds) {
    // Match 0x18FF5000 through 0x18FF50FF.
    canReady = CAN.filterExtended(0x18FF5000UL, 0x1FFFFF00UL);
  } else {
    canReady = CAN.filter(0x120, 0x7F0);
  }
  if (!canReady) Serial.println("CAN filter configuration failed");
}

void loop() {
  if (canReady && CAN.available()) {
    Serial.println(CAN.read());
  }
}
