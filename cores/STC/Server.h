#ifndef STCXX_SERVER_H
#define STCXX_SERVER_H

#include "Print.h"

class Server : public Print
{
public:
    virtual void begin() = 0;
    using Print::write;
};

namespace arduino {
using ::Server;
} // namespace arduino

#endif
