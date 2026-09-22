/* SPDX-License-Identifier: MIT */
#include "HID.h"
HID_ &HID() { static HID_ instance; return instance; }
