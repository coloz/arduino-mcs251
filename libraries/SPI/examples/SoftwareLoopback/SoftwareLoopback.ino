#include <Arduino.h>
#include <SPI.h>

void setup(void)
{
    uint8_t received;

    Serial.begin(9600UL);
    SPI.begin();
    SPI.beginTransaction(SPISettings(100000UL, MSBFIRST, SPI_MODE0));

    digitalWrite(SS, LOW);
    received = SPI.transfer(0x5au);
    digitalWrite(SS, HIGH);

    SPI.endTransaction();
    Serial.write(received);
}

void loop(void)
{
    delay(1000UL);
}
