/* SPDX-License-Identifier: MIT */
#ifndef STC_CAN_PACKET_H
#define STC_CAN_PACKET_H
#include "CAN.h"
/* Stream adapter for the community arduino-CAN packet API. The official CAN
 * object keeps read()->CanMsg and available()->frame count. */
class CANPacketClass : public Stream {
public:
    explicit CANPacketClass(CANClass &controller = CAN)
        : _can(controller), _tx(), _rx(), _position(0), _size(0), _dlc(-1), _writing(false) {}
    int begin(long bitrate) { return bitrate > 0 && _can.begin((uint32_t)bitrate); }
    void end() { _can.end(); _writing = false; _position = 0; _rx = CanMsg(); }
    void setPins(uint8_t rx, uint8_t tx) { _can.setPins(rx, tx); }
    int beginPacket(int id, int dlc = -1, bool rtr = false) { return start((uint32_t)id, dlc, rtr, false); }
    int beginExtendedPacket(long id, int dlc = -1, bool rtr = false) { return start((uint32_t)id, dlc, rtr, true); }
    size_t write(uint8_t value) override {
        if (!_writing || _tx.isRemoteFrame() || _size >= 8 || (_dlc >= 0 && _size >= _dlc)) {
            setWriteError(); return 0;
        }
        _tx.data[_size++] = value; return 1;
    }
    using Print::write;
    int endPacket() {
        if (!_writing) return 0;
        _tx.data_length = _dlc < 0 ? _size : (uint8_t)_dlc;
        _writing = false; return _can.write(_tx) == 1;
    }
    int parsePacket() {
        _position = 0; _rx = CanMsg();
        return _can.read(_rx) ? _rx.data_length : 0;
    }
    long packetId() const { return (long)_rx.getExtendedId(); }
    bool packetExtended() const { return _rx.isExtendedId(); }
    bool packetRtr() const { return _rx.isRemoteFrame(); }
    int packetDlc() const { return _rx.data_length; }
    int available() override { return _rx.isRemoteFrame() ? 0 : _rx.data_length - _position; }
    int peek() override { return available() ? _rx.data[_position] : -1; }
    int read() override { return available() ? _rx.data[_position++] : -1; }
    void flush() override {}
    int filter(int id, int mask = 0x7ff) { return _can.filter((uint32_t)id, (uint32_t)mask); }
    int filterExtended(long id, long mask = 0x1fffffffL) { return _can.filterExtended((uint32_t)id, (uint32_t)mask); }
private:
    int start(uint32_t id, int dlc, bool rtr, bool extended) {
        _writing = false;
        if (id > (extended ? 0x1fffffffUL : 0x7ffUL) || dlc < -1 || dlc > 8) return 0;
        _tx = CanMsg(); _tx.id = id | (extended ? CanMsg::CAN_EFF_FLAG : 0UL) | (rtr ? CanMsg::CAN_RTR_FLAG : 0UL);
        _size = 0; _dlc = dlc; _writing = true; return 1;
    }
    CANClass &_can;
    CanMsg _tx, _rx;
    uint8_t _position, _size;
    int _dlc;
    bool _writing;
};
#endif
