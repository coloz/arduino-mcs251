/*
 * SPDX-License-Identifier: MIT
 *
 * Process-wide owner for the SoftwareSerial single-active-port C HAL.
 */

#include "SoftwareSerial.h"

SoftwareSerial *SoftwareSerial::_activeObject = 0;
