#ifndef STCXX_STREAM_H
#define STCXX_STREAM_H

#include <stddef.h>
#include <stdint.h>

#include "Print.h"

enum LookaheadMode {
    SKIP_ALL,
    SKIP_NONE,
    SKIP_WHITESPACE
};

#define NO_IGNORE_CHAR '\x01'

class Stream : public Print
{
public:
    Stream() : _timeout(1000UL), _startMillis(0UL) {}

    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    void setTimeout(unsigned long timeout) { _timeout = timeout; }
    unsigned long getTimeout() const { return _timeout; }

    bool find(const char *target);
    bool find(const uint8_t *target)
    {
        return find(reinterpret_cast<const char *>(target));
    }
    bool find(const char *target, size_t length);
    bool find(const uint8_t *target, size_t length)
    {
        return find(reinterpret_cast<const char *>(target), length);
    }
    bool find(char target) { return find(&target, 1u); }
    bool findUntil(const char *target, const char *terminator);
    bool findUntil(const uint8_t *target, const char *terminator)
    {
        return findUntil(reinterpret_cast<const char *>(target), terminator);
    }
    bool findUntil(const char *target, size_t targetLength,
                   const char *terminator, size_t terminatorLength);
    bool findUntil(const uint8_t *target, size_t targetLength,
                   const char *terminator, size_t terminatorLength)
    {
        return findUntil(reinterpret_cast<const char *>(target), targetLength,
                         terminator, terminatorLength);
    }

    long parseInt(LookaheadMode lookahead = SKIP_ALL,
                  char ignore = NO_IGNORE_CHAR);
    float parseFloat(LookaheadMode lookahead = SKIP_ALL,
                     char ignore = NO_IGNORE_CHAR);

    size_t readBytes(char *buffer, size_t length);
    size_t readBytes(uint8_t *buffer, size_t length)
    {
        return readBytes(reinterpret_cast<char *>(buffer), length);
    }
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    size_t readBytesUntil(char terminator, uint8_t *buffer, size_t length)
    {
        return readBytesUntil(terminator, reinterpret_cast<char *>(buffer),
                              length);
    }
    String readString();
    String readStringUntil(char terminator);

protected:
    unsigned long _timeout;
    unsigned long _startMillis;

    int timedRead();
    int timedPeek();
    int peekNextDigit(LookaheadMode lookahead, bool detectDecimal);
    long parseInt(char ignore) { return parseInt(SKIP_ALL, ignore); }
    float parseFloat(char ignore) { return parseFloat(SKIP_ALL, ignore); }

    struct MultiTarget {
        // Official names plus the original STC spellings, sharing storage.
        union { const char *str; const char *text; };
        union { size_t len; size_t length; };
        size_t index;
    };

    int findMulti(MultiTarget *targets, int targetCount);
};

#undef NO_IGNORE_CHAR

namespace arduino {
using ::Stream;
using ::LookaheadMode;
using ::SKIP_ALL;
using ::SKIP_NONE;
using ::SKIP_WHITESPACE;
}

#endif
