/* SPDX-License-Identifier: MIT */
#ifndef STC_HARDWARE_CAN_H
#define STC_HARDWARE_CAN_H
#include "CanMsg.h"
// MCS251 int is 16 bits; use a 32-bit underlying type for bit rates.
enum class CanBitRate : uint32_t {
    BR_125k = 125000UL, BR_250k = 250000UL,
    BR_500k = 500000UL, BR_1000k = 1000000UL
};
namespace arduino {
class HardwareCAN {
public:
    virtual ~HardwareCAN() {}
    virtual bool begin(CanBitRate bitrate) = 0;
    virtual void end() = 0;
    virtual int write(const CanMsg &message) = 0;
    virtual size_t available() = 0;
    // Preserve Arduino's read()->CanMsg call surface. The STC bridge cannot
    // carry aggregate-return functions in vtables; dispatch through an output
    // parameter internally instead. This also distinguishes empty/zero-DLC RX.
    CanMsg read() { CanMsg message; read(message); return message; }
    virtual bool read(CanMsg &message) = 0;
};
}
using arduino::HardwareCAN;
#endif
