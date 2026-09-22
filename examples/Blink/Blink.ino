#include <Arduino.h>

/* Connect an LED and series resistor to P3.2; adjust for your board's wiring. */
static const uint8_t ledPin = P3_2;

void setup(void)
{
    pinMode(ledPin, OUTPUT);
}

void loop(void)
{
    digitalWrite(ledPin, HIGH);
    delay(500UL);
    digitalWrite(ledPin, LOW);
    delay(500UL);
}
