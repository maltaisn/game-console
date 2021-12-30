
# These values are defined in the code and will be added to CFLAGS with -D prefix.

# core firmware version
DEFINES += VERSION_MAJOR=0
DEFINES += VERSION_MINOR=2

# baud rates used for uart link in debug port.
# by default the baud rate is slower, but a faster baud rate
# can be enabled with the "fast mode" packet.
DEFINES += UART_BAUD=19200
DEFINES += UART_BAUD_FAST=1000000

# size of the display buffer in bytes, must be a multiple of 64.
# DEFINES += DISPLAY_BUFFER_SIZE=2048

# disables auto shutdown for battery overdischarge protection.
# DEFINES += DISABLE_BAT_PROT
