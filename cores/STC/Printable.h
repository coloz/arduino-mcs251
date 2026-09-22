#ifndef STCXX_PRINTABLE_H
#define STCXX_PRINTABLE_H

#include <stddef.h>

class Print;

class Printable
{
public:
    virtual size_t printTo(Print &output) const = 0;
};

namespace arduino {
using ::Print;
using ::Printable;
}

#endif
