/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include <SD.h>

void setup(void)
{
    uint8_t chunk[16];
    size_t count;

    Serial.begin(115200UL);

    if (SD.setPins(MOSI, MISO, SCK, SS) == 0u || SD.begin(SS) == 0u) {
        Serial.println("SD init or FAT mount failed");
        Serial.print((long)SD.error(), DEC);
        Serial.write((uint8_t)'\n');
        return;
    }

    Serial.print("card=");
    Serial.print((long)SD.cardType(), DEC);
    Serial.print(" fat=");
    Serial.println((long)SD.fatType(), DEC);

    File file = SD.open("README.TXT", FILE_READ);
    if (!file) {
        Serial.println("README.TXT not found");
        return;
    }

    Serial.print("size=");
    Serial.println((long)file.size(), DEC);
    (void)file.peek();
    count = file.readBytes(chunk, sizeof(chunk));
    if (count != 0u) {
        (void)Serial.write(chunk[0]);
    }
    (void)file.seek(0UL);

    while (file.available() != 0UL) {
        int value = file.read();
        if (value < 0) {
            break;
        }
        (void)Serial.write((uint8_t)value);
    }
    Serial.print("\nposition=");
    Serial.println((long)file.position(), DEC);
    file.close();
}

void loop(void)
{
    delay(1000UL);
}
