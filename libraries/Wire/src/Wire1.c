/* I2C2 exists only on variants that advertise two hardware controllers. */
#include "Arduino.h"
#if STC_CORE_I2C_COUNT > 1
#define STC_WIRE_INSTANCE 1
#define STC_WIRE_STATE_IN_DATA 0
#define WIRE_DEFAULT_SDA_PIN PIN_WIRE1_SDA
#define WIRE_DEFAULT_SCL_PIN PIN_WIRE1_SCL
#include "Wire1_aliases.h"
#include "WireInstance.h"
#endif
