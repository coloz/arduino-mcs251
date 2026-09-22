/*
 * SPDX-License-Identifier: MIT
 *
 * Arduino-compatible C++ facade for the blocking STC stepper HAL.
 */
#ifndef STC_STEPPER_CLASS_H
#define STC_STEPPER_CLASS_H

class Stepper
{
public:
    Stepper(int stepsPerRevolution, uint8_t pin1, uint8_t pin2)
        : _state(), _status(STEPPER_STATUS_NOT_CONFIGURED)
    {
        Context context(&_state);
        _status = Stepper_setPins2(normalizeSteps(stepsPerRevolution), pin1, pin2);
    }

    Stepper(int stepsPerRevolution, uint8_t pin1, uint8_t pin2,
            uint8_t pin3, uint8_t pin4)
        : _state(), _status(STEPPER_STATUS_NOT_CONFIGURED)
    {
        Context context(&_state);
        _status = Stepper_setPins4(normalizeSteps(stepsPerRevolution), pin1, pin2, pin3, pin4);
    }

    Stepper(int stepsPerRevolution, uint8_t pin1, uint8_t pin2,
            uint8_t pin3, uint8_t pin4, uint8_t pin5)
        : _state(), _status(STEPPER_STATUS_NOT_CONFIGURED)
    {
        Context context(&_state);
        _status = Stepper_setPins5(normalizeSteps(stepsPerRevolution), pin1, pin2, pin3, pin4, pin5);
    }

    void setSpeed(long rpm) { Context context(&_state); _status = Stepper_setSpeed(rpm); }
    void step(int stepsToMove) { Context context(&_state); Stepper_step((long)stepsToMove); }
    int version() { return 5; }
    void release() { Context context(&_state); Stepper_release(); }
    uint8_t status() const { return _status; }
    explicit operator bool() const
    {
        return _status == STEPPER_STATUS_SUCCESS;
    }

private:
    class Context {
        STCStepperState *_previous;
    public:
        explicit Context(STCStepperState *state) : _previous(Stepper_selectContext(state)) {}
        ~Context() { Stepper_selectContext(_previous); }
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;
    };
    static unsigned long normalizeSteps(int value)
    {
        return value > 0 ? (unsigned long)value : 0UL;
    }

    STCStepperState _state;
    uint8_t _status;
};

#endif
