# SPI

The public Arduino SPI API retains transaction interrupt guards, configurable
pins, supported hardware routes and the software fallback. Chip selects remain
under application control; devices sharing a bus need distinct selects.

On STC32G12K128, STC32G144K246, AI8051U-34K64, STC32G12K64, STC32G8K64 and
STC32CL8K64, eight bytes of frequently accessed bus state use DATA to reduce
Flash spent accessing XDATA. Transaction bookkeeping keeps its original
placement. To revert, pass `-DSTC_SPI_STATE_IN_DATA=0` through platform-wide
`build.extra_flags`; defining it only in a sketch cannot configure SPI.c.

