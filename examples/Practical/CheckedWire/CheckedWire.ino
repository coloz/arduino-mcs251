#include <Arduino.h>
#include <Wire.h>

/* SDA=P3.2, SCL=P3.3, external pull-ups and common ground required. */
/* The 16 KiB part uses LED/status feedback to leave Flash for the I2C driver.
 * wireStatus remains available to a debugger; LED on means the probe ACKed. */
volatile uint8_t wireStatus;
static bool wireReady;
void setup(void) {
#if !defined(AI8051U_34K16)
    Serial.begin(115200UL);
#endif
    pinMode(LED_BUILTIN, OUTPUT);
    wireStatus = Wire.setPinsChecked(P3_2, P3_3);
    if (wireStatus != WIRE_STATUS_SUCCESS) {
#if !defined(AI8051U_34K16)
        Serial.println("Invalid I2C pin configuration");
#endif
        return;
    }
    Wire.begin(); Wire.setWireTimeout(25000UL, 1);
    wireReady = true;
}
void loop(void) {
    uint8_t status;
    if (!wireReady) return;
    Wire.beginTransmission(0x3c);
    status = Wire.endTransmission();
    wireStatus = status;
    digitalWrite(LED_BUILTIN, status == WIRE_STATUS_SUCCESS ? HIGH : LOW);
#if !defined(AI8051U_34K16)
    if (status == WIRE_STATUS_SUCCESS) Serial.println("0x3c ACK");
    else if (status == WIRE_STATUS_TIMEOUT) Serial.println("I2C timeout");
    else Serial.println("I2C transaction failed");
#endif
    delay(500);
}
