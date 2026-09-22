#include <Arduino.h>
#include <Wire.h>

#define I2C_DEVICE_ADDRESS 0x50u
#define I2C_REGISTER       0x00u

void setup(void)
{
    Serial.begin(9600UL);
    Wire.begin();
    Wire.setClock(100000UL);

    Wire.beginTransmission(I2C_DEVICE_ADDRESS);
    Wire.write(I2C_REGISTER);
    if (Wire.endTransmission(false) == WIRE_STATUS_SUCCESS) {
        Wire.requestFrom(I2C_DEVICE_ADDRESS, 1u);
    }
}

void loop(void)
{
    int value = Wire.read();

    if (value >= 0) {
        Serial.write((uint8_t)value);
    }
    delay(1000UL);
}
