
#include "Stream.h"

#include <limits.h>

#include "stcxx_libc.h"

#include "hal/stc_c_hal.h"

namespace {

size_t stcxx_stream_border_length(const char *text, size_t matchedLength)
{
    size_t candidate = matchedLength;
    while (candidate-- != 0u) {
        if (memcmp(text, text + matchedLength - candidate, candidate) == 0) {
            return candidate;
        }
    }
    return 0u;
}

} // namespace

int Stream::timedRead()
{
    int value;
    _startMillis = millis();
    do {
        value = read();
        if (value >= 0) {
            return value;
        }
        yield();
    } while ((unsigned long)(millis() - _startMillis) < _timeout);
    return -1;
}

int Stream::timedPeek()
{
    int value;
    _startMillis = millis();
    do {
        value = peek();
        if (value >= 0) {
            return value;
        }
        yield();
    } while ((unsigned long)(millis() - _startMillis) < _timeout);
    return -1;
}

int Stream::peekNextDigit(LookaheadMode lookahead, bool detectDecimal)
{
    for (;;) {
        int value = timedPeek();
        if (value < 0 || value == '-' || (value >= '0' && value <= '9') ||
            (detectDecimal && value == '.')) {
            return value;
        }
        if (lookahead == SKIP_NONE) {
            return -1;
        }
        if (lookahead == SKIP_WHITESPACE && value != ' ' && value != '\t' &&
            value != '\r' && value != '\n') {
            return -1;
        }
        (void)read();
    }
}

bool Stream::find(const char *target)
{
    return target != 0 ? find(target, strlen(target)) : false;
}

bool Stream::find(const char *target, size_t length)
{
    return findUntil(target, length, 0, 0u);
}

bool Stream::findUntil(const char *target, const char *terminator)
{
    if (target == 0) {
        return false;
    }
    return findUntil(target, strlen(target), terminator,
                     terminator == 0 ? 0u : strlen(terminator));
}

bool Stream::findUntil(const char *target, size_t targetLength,
                       const char *terminator, size_t terminatorLength)
{
    MultiTarget targets[2];
    int count = 1;
    if (target == 0) {
        return false;
    }
    targets[0].text = target;
    targets[0].length = targetLength;
    targets[0].index = 0u;
    if (terminator != 0) {
        targets[1].text = terminator;
        targets[1].length = terminatorLength;
        targets[1].index = 0u;
        count = 2;
    }
    return findMulti(targets, count) == 0;
}

int Stream::findMulti(MultiTarget *targets, int targetCount)
{
    // Public find/findUntil use at most two patterns. Keep short-pattern KMP
    // failure links on the stack; long/custom MultiTarget searches retain the
    // allocation-free fallback. No changes to byte consumption or timeouts.
    uint8_t borders[2][32];
    if (targets == 0 || targetCount <= 0) {
        return -1;
    }
    for (int targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
        if (targets[targetIndex].text == 0) {
            return -1;
        }
        if (targets[targetIndex].length == 0u) {
            return targetIndex;
        }
        if (targetIndex < 2 && targets[targetIndex].length <= 32u) {
            const char *text = targets[targetIndex].text;
            uint8_t matched = 0u;
            borders[targetIndex][0] = 0u;
            for (uint8_t i = 1u; i < targets[targetIndex].length; ++i) {
                while (matched && text[i] != text[matched]) {
                    matched = borders[targetIndex][matched - 1u];
                }
                if (text[i] == text[matched]) ++matched;
                borders[targetIndex][i] = matched;
            }
        }
    }
    for (;;) {
        int input = timedRead();
        int targetIndex;
        if (input < 0) {
            return -1;
        }
        for (targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
            MultiTarget &target = targets[targetIndex];
            while (target.index != 0u &&
                   (char)input != target.text[target.index]) {
                target.index = targetIndex < 2 && target.length <= 32u
                    ? borders[targetIndex][target.index - 1u]
                    : stcxx_stream_border_length(target.text, target.index);
            }
            if ((char)input == target.text[target.index]) {
                ++target.index;
                if (target.index == target.length) {
                    return targetIndex;
                }
            }
        }
    }
}

long Stream::parseInt(LookaheadMode lookahead, char ignore)
{
    bool negative = false;
    unsigned long value = 0UL;
    int current = peekNextDigit(lookahead, false);
    if (current < 0) {
        return 0L;
    }
    do {
        if (current == ignore) {
            (void)read();
        } else if (current == '-') {
            negative = true;
            (void)read();
        } else if (current >= '0' && current <= '9') {
            value = value * 10UL + (unsigned long)(current - '0');
            (void)read();
        } else {
            break;
        }
        current = timedPeek();
    } while ((current >= '0' && current <= '9') || current == ignore);
    // Accumulate modulo the word width: LONG_MIN's positive magnitude does
    // not fit in a signed long. Convert back without signed overflow or an
    // out-of-range unsigned-to-signed cast, including for overlong input.
    if (negative) {
        value = 0UL - value;
    }
    return value <= (unsigned long)LONG_MAX ? (long)value
                                           : -1L - (long)(~value);
}

float Stream::parseFloat(LookaheadMode lookahead, char ignore)
{
    bool negative = false;
    bool decimalSeen = false;
    double value = 0.0;
    double scale = 1.0;
    int current = peekNextDigit(lookahead, true);
    if (current < 0) {
        return 0.0f;
    }
    do {
        if (current == ignore) {
            (void)read();
        } else if (current == '-') {
            negative = true;
            (void)read();
        } else if (current == '.' && !decimalSeen) {
            decimalSeen = true;
            (void)read();
        } else if (current >= '0' && current <= '9') {
            if (decimalSeen) {
                scale *= 0.1;
                value += scale * (current - '0');
            } else {
                value = value * 10.0 + (current - '0');
            }
            (void)read();
        } else {
            break;
        }
        current = timedPeek();
    } while ((current >= '0' && current <= '9') ||
             (current == '.' && !decimalSeen) || current == ignore);
    return negative ? -value : value;
}

size_t Stream::readBytes(char *buffer, size_t length)
{
    size_t count = 0u;
    if (buffer == 0) {
        return 0u;
    }
    while (count < length) {
        int value = timedRead();
        if (value < 0) {
            break;
        }
        buffer[count++] = (char)value;
    }
    return count;
}

size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length)
{
    size_t count = 0u;
    if (buffer == 0) {
        return 0u;
    }
    while (count < length) {
        int value = timedRead();
        if (value < 0 || value == (unsigned char)terminator) {
            break;
        }
        buffer[count++] = (char)value;
    }
    return count;
}

String Stream::readString()
{
    String result;
    for (;;) {
        int value = timedRead();
        if (value < 0 || result.concat((char)value) == 0u) {
            return result;
        }
    }
}

String Stream::readStringUntil(char terminator)
{
    String result;
    for (;;) {
        int value = timedRead();
        if (value < 0 || value == (unsigned char)terminator ||
            result.concat((char)value) == 0u) {
            return result;
        }
    }
}
