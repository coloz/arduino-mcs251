/*
  Keyboard_es_LATAM.h -- Latin American Spanish keyboard

  Copyright (c) 2026, WrenchPC

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef KEYBOARD_ES_LATAM_h
#define KEYBOARD_ES_LATAM_h

#include "HID.h"

#if !defined(_USING_HID)

#warning "Using legacy HID core (non pluggable)"

#else

//================================================================================
//================================================================================
//  Keyboard

// es_LATAM keys
#define KEY_INVERTED_QUESTION    (136+0x2e)   // ¿
#define KEY_ACUTE                (136+0x2f)   // dead acute accent
#define KEY_N_TILDE              (136+0x33)   // Ñ

#endif
#endif
