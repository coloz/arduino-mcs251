# Arduino binary constants notice

`cores/STC/binary.h` is derived from the Arduino binary-constant definitions
authored by David A. Mellis (copyright 2006). It is distributed under the GNU
Lesser General Public License, version 2.1 or, at your option, any later
version. The complete LGPL-2.1 text is included as `LICENSES/LGPL-2.1.txt`.

# Arduino Keyboard and Mouse

`libraries/Keyboard/src` and `libraries/Mouse/src` are derived from the
Arduino libraries under GNU LGPL version 2.1 or later. Their original source
copyright notices are retained. The complete license texts are distributed as
`libraries/Keyboard/LICENSE`, `libraries/Mouse/LICENSE.txt`, and
`LICENSES/LGPL-2.1.txt`.

Upstream revisions:

- https://github.com/arduino-libraries/Keyboard/tree/3f7bad0a41839689684e3b46ce9deb0232f8ec2d
- https://github.com/arduino-libraries/Mouse/tree/6a0478972dfb499ddb1b5b4cda9884fbdd1043ad

STC changes initialize the native HID transport, register initial input reports,
release buttons on end, expose Print overloads, propagate keyboard send errors,
and add keyboard LED output reports. Details are recorded in each library README.
The adapted source is provided in full for rebuilding with the platform toolchain.

The STC USB transport (`libraries/HID`), CAN implementation (`libraries/CAN`),
and new core service hook are original project code under the root MIT license.
Vendor register documentation was consulted; no proprietary STC library binary
is included or required.

# STC upstream asset notice

No STC SDK archive, proprietary prebuilt `.LIB`, or AiCube-ISP binary is distributed in this repository. Arduino installation, compilation and upload do not require those vendor downloads.

Users who choose to obtain additional vendor files should refer to the license or EULA supplied by STC.

The SDCC MCS-251 compiler is distributed as a separate Arduino tool dependency. Its source code and license notices accompany the compiler releases.
