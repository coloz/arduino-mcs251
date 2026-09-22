#ifndef STC_CORE_WCHARACTER_H
#define STC_CORE_WCHARACTER_H

#include <stdbool.h>
#include <ctype.h>

/* Arduino's character helpers are boolean-valued wrappers around ctype.
 * Inline functions (rather than macros) coexist with String::toLowerCase()
 * and String::toUpperCase(). isAscii()/toAscii() stay local because the
 * target libc does not promise the non-standard POSIX functions. */
static inline bool isAlphaNumeric(int value) { return isalnum(value) != 0; }
static inline bool isAlpha(int value) { return isalpha(value) != 0; }
static inline bool isAscii(int value)
{
    return value >= 0 && value <= 0x7f;
}
static inline bool isWhitespace(int value)
{
    return value == ' ' || value == '\t';
}
static inline bool isControl(int value) { return iscntrl(value) != 0; }
static inline bool isDigit(int value) { return isdigit(value) != 0; }
static inline bool isGraph(int value) { return isgraph(value) != 0; }
static inline bool isLowerCase(int value) { return islower(value) != 0; }
static inline bool isPrintable(int value) { return isprint(value) != 0; }
static inline bool isPunct(int value) { return ispunct(value) != 0; }
static inline bool isSpace(int value) { return isspace(value) != 0; }
static inline bool isUpperCase(int value) { return isupper(value) != 0; }
static inline bool isHexadecimalDigit(int value)
{
    return isxdigit(value) != 0;
}
static inline int toAscii(int value) { return value & 0x7f; }
static inline int toLowerCase(int value) { return tolower(value); }
static inline int toUpperCase(int value) { return toupper(value); }

#endif
