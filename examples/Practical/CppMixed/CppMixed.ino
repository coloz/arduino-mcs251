#include <Arduino.h>
#include <new>

extern "C" uint16_t native_add(uint16_t left, uint16_t right);

// Exercise global construction, templates, virtual dispatch and the C ABI.
struct CounterBase {
    virtual uint16_t next() = 0;
    virtual ~CounterBase() { }
};

template<uint16_t Start> struct Counter : CounterBase {
    uint16_t value;
    Counter() : value(native_add(Start, 0)) { }
    uint16_t next() override { return value = native_add(value, 1); }
};

Counter<41> counter;
CounterBase *volatile activeCounter = &counter;

void setup() { Serial.begin(115200); }

void loop() {
    uint16_t *value = new (std::nothrow) uint16_t(activeCounter->next());
    if (value) Serial.println(*value);
    delete value;
    delay(1000);
}
