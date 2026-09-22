#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  // Timer2 owns one output at a time. Timer0 and UART1 remain available.
  if (toneChecked(P3_2, 1000, 250) != STC_TONE_OK)
    Serial.println(toneConfigurationError());
}
void loop() {
  delay(1000);
  tone(P3_2, 500, 250);
  delay(1000);
  noTone(P3_2);
}
