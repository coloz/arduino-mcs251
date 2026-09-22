#include <Wire.h>

// Hardware slave route: SDA=P3.3, SCL=P3.2; use external pull-ups.
volatile uint8_t lastByte;
void receiveBytes(int count) {
  while (count-- && Wire.available()) lastByte = (uint8_t)Wire.read();
}
void requestBytes() { Wire.write((uint8_t)lastByte); }
void setup() {
  Wire.setPins(P3_3, P3_2);
  Wire.onReceive(receiveBytes);
  Wire.onRequest(requestBytes);
  Wire.begin(0x2a);
}
void loop() {}
