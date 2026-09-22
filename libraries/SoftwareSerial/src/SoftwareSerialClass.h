/*
 * SPDX-License-Identifier: MIT
 *
 * Arduino-compatible C++ facade for the polling STC software-UART HAL.
 */
#ifndef STC_SOFTWARE_SERIAL_CLASS_H
#define STC_SOFTWARE_SERIAL_CLASS_H

class SoftwareSerial : public Stream
{
public:
    SoftwareSerial(uint8_t receivePin, uint8_t transmitPin,
                   bool inverseLogic = false)
        : _receivePin(receivePin), _transmitPin(transmitPin),
          _baud(0UL), _inverseLogic(inverseLogic), _started(false)
    {
    }

    SoftwareSerial(const SoftwareSerial &other)
        : _receivePin(other._receivePin), _transmitPin(other._transmitPin),
          _baud(other._baud), _inverseLogic(other._inverseLogic),
          _started(false)
    {
    }

    SoftwareSerial &operator=(const SoftwareSerial &other)
    {
        if (this != &other) {
            end();
            _receivePin = other._receivePin;
            _transmitPin = other._transmitPin;
            _baud = other._baud;
            _inverseLogic = other._inverseLogic;
            _started = false;
        }
        return *this;
    }

    ~SoftwareSerial()
    {
        end();
    }

    void begin(unsigned long baud)
    {
        (void)activate(baud);
    }

    void end()
    {
        if (ownsBackend()) {
            SoftwareSerial_end();
            _activeObject = 0;
        }
        _started = false;
    }

    bool listen()
    {
        if (_baud == 0UL) {
            return false;
        }
        if (!ownsBackend()) {
            return activate(_baud);
        }
        if (SoftwareSerial_isListening()) {
            return true;
        }
        return SoftwareSerial_listen();
    }

    bool stopListening()
    {
        return ownsBackend() && SoftwareSerial_stopListening();
    }
    bool isListening()
    {
        return ownsBackend() && SoftwareSerial_isListening();
    }

    int available() override
    {
        return ownsBackend() ? SoftwareSerial_available() : 0;
    }
    int availableForWrite() override
    {
        return ownsBackend() ? SoftwareSerial_availableForWrite() : 0;
    }
    int peek() override
    {
        return ownsBackend() ? SoftwareSerial_peek() : -1;
    }
    int read() override
    {
        return ownsBackend() ? SoftwareSerial_read() : -1;
    }
    void flush() override
    {
        if (ownsBackend()) {
            SoftwareSerial_flush();
        }
    }

    size_t write(uint8_t value) override
    {
        if (!ownsBackend()) {
            setWriteError();
            return 0u;
        }
        size_t written = SoftwareSerial_write(value);
        if (written == 0u) {
            setWriteError();
        }
        return written;
    }
    using Print::write;

    size_t poll()
    {
        return ownsBackend() ? SoftwareSerial_poll() : 0u;
    }
    bool overflow()
    {
        return ownsBackend() && SoftwareSerial_overflow();
    }
    bool framingError()
    {
        return ownsBackend() && SoftwareSerial_framingError();
    }
    bool timingError()
    {
        return ownsBackend() && SoftwareSerial_timingError();
    }

    explicit operator bool() const { return ownsBackend(); }

private:
    bool ownsBackend() const
    {
        return _started && _activeObject == this;
    }

    bool activate(unsigned long baud)
    {
        SoftwareSerial *previous = _activeObject;

        if (!SoftwareSerial_beginOnPins(_receivePin, _transmitPin,
                                        _inverseLogic, baud)) {
            if (previous != this) {
                _started = false;
            }
            return false;
        }
        if (previous != 0 && previous != this) {
            previous->_started = false;
        }
        _activeObject = this;
        _baud = baud;
        _started = true;
        return true;
    }

    static SoftwareSerial *_activeObject;
    uint8_t _receivePin;
    uint8_t _transmitPin;
    unsigned long _baud;
    bool _inverseLogic;
    bool _started;
};

#endif
