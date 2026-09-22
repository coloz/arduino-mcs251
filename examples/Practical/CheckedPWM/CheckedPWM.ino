#include <Arduino.h>

/* AI8051U: P5.2 supports PWM. Select a supported pin on other models.
 * An active-low LED gets brighter as this duty value gets smaller. */
#define PWM_PIN P5_2
static uint8_t available;
void setup(void) {
    Serial.begin(115200UL);
    available = digitalPinHasPWM(PWM_PIN);
    if (!available) Serial.println("Selected pin has no mapped PWM channel");
}
void loop(void) {
    static uint8_t duty = 1;
    if (available) {
        if (analogWriteChecked(PWM_PIN, duty) != STC_PWM_OK)
            Serial.println("PWM request failed");
        if (++duty == 255) duty = 1;
    }
    delay(10);
}
