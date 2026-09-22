/*
 * Supported chips:
 *   STC32G12K64, STC32G12K128, STC32G144K246,
 *   STC32G8K48, STC32G8K64, STC32CL8K48, STC32CL8K64.
 */

#include <Arduino_CAN.h>

// Connect two CAN nodes at 500 kbit/s. Set requester=false on the responder.
const bool requester = true;
const uint32_t requestId = 0x456;
bool canReady = false;
bool replyPending = false;
unsigned long lastRequest = 0;
uint8_t sequence = 0;
CanMsg reply;

void setup() {
  Serial.begin(115200);
  canReady = CAN.begin(CanBitRate::BR_500k);
  if (!canReady) Serial.println("CAN initialization failed");
}

void loop() {
  if (!canReady) return;

  if (requester && millis() - lastRequest >= 1000) {
    lastRequest = millis();
    CanMsg request;
    request.id = CanStandardId(requestId) | CanMsg::CAN_RTR_FLAG;
    request.data_length = 2;  // Requested DLC; an RTR frame has no payload.
    int result = CAN.write(request);
    if (result < 0) {
      Serial.print("Request error: ");
      Serial.println(result);
    }
  }

  CanMsg incoming;
  if (CAN.read(incoming) && incoming.isStandardId() &&
      incoming.getStandardId() == requestId) {
    if (requester && !incoming.isRemoteFrame()) {
      Serial.print("Response: ");
      Serial.println(incoming);
    } else if (!requester && incoming.isRemoteFrame() &&
               incoming.data_length == 2 && !replyPending) {
      const uint8_t payload[] = {sequence++, 0x51};
      reply = CanMsg(CanStandardId(requestId), sizeof(payload), payload);
      replyPending = true;
    }
  }

  if (replyPending) {
    int result = CAN.write(reply);
    if (result == 1) replyPending = false;
    else if (result != -STC_CAN_BUSY) {
      Serial.print("Reply error: ");
      Serial.println(result);
      replyPending = false;
    }
  }
}
