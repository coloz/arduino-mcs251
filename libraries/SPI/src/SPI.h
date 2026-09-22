/* SPDX-License-Identifier: MIT */
#ifndef STC_SPI_H
#define STC_SPI_H

#ifndef __cplusplus
# error "SPI.h requires C++"
#endif

#ifndef SPI_HAS_TRANSACTION
# define SPI_HAS_TRANSACTION 1
#endif
#ifndef SPI_HAS_NOTUSINGINTERRUPT
# define SPI_HAS_NOTUSINGINTERRUPT 1
#endif
#ifndef SPI_ATOMIC_VERSION
# define SPI_ATOMIC_VERSION 1
#endif

#include "SPIClass.h"

#endif
