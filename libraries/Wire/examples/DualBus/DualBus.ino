#include <Wire.h>

#if STC_CORE_I2C_COUNT < 2
#error "DualBus requires two hardware I2C controllers (STC32G144K246)"
#endif

void setup()
{
    Wire.begin();                  // I2C1: SDA=P3.3, SCL=P3.2
    Wire1.begin();                 // I2C2: SDA=P2.6, SCL=P2.7
    Wire.setClock(100000);
    Wire1.setClock(400000);
}

void loop()
{
    // Both buses may have a device at the same address. Add external pull-ups.
    Wire.beginTransmission(0x50);
    Wire.write((uint8_t)0);
    Wire.endTransmission();
    Wire1.beginTransmission(0x50);
    Wire1.write((uint8_t)0);
    Wire1.endTransmission();
    delay(1000);
}
