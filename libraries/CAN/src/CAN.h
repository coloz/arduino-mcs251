/* SPDX-License-Identifier: MIT */
#ifndef STC_CAN_H
#define STC_CAN_H
#include "HardwareCAN.h"
#include "CAN_backend.h"

class CANClass : public HardwareCAN {
public:
    explicit CANClass(uint8_t bus = 0) : _bus(bus), _rx(0xff), _tx(0xff) {}
    bool begin(CanBitRate bitrate) override { return begin((uint32_t)bitrate); }
    bool begin(uint32_t bitrate) { return stc_can_begin(_bus, bitrate, _rx, _tx) != 0; }
    void end() override { stc_can_end(_bus); }
    int write(const CanMsg &message) override;
    size_t available() override { return stc_can_available(_bus); }
    using HardwareCAN::read;
    bool read(CanMsg &message) override;
    // Configure before begin(). Only documented peripheral routes are accepted.
    void setPins(uint8_t rx, uint8_t tx) { _rx = rx; _tx = tx; }
    uint8_t lastError() const { return stc_can_error(_bus); }
    bool filter(uint32_t id, uint32_t mask = 0x7ffUL) { return stc_can_filter(_bus, id, mask, 0) != 0; }
    bool filterExtended(uint32_t id, uint32_t mask = 0x1fffffffUL) { return stc_can_filter(_bus, id, mask, 1) != 0; }
private:
    uint8_t _bus, _rx, _tx;
};
extern CANClass CAN;
extern CANClass CAN1;
#endif
