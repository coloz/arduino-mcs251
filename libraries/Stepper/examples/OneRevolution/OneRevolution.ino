/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include <Stepper.h>

#define MOTOR_STEPS_PER_REVOLUTION 200UL

/* Drive the motor through a suitable transistor/driver stage.  Never connect
 * a motor winding directly to an MCU pin. */

Stepper motor(MOTOR_STEPS_PER_REVOLUTION, P3_0, P3_1, P3_2, P3_3);

void setup(void)
{
    motor.setSpeed(30L);
}

void loop(void)
{
    if (!motor) {
        delay(1000UL);
        return;
    }
    motor.step((int)MOTOR_STEPS_PER_REVOLUTION);
    delay(500UL);
    motor.step(-(int)MOTOR_STEPS_PER_REVOLUTION);
    delay(500UL);
    motor.release();
    delay(1000UL);
}
