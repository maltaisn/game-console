
# These values are defined in the code and will be added to CFLAGS with -D prefix.

# core firmware version
DEFINES += VERSION_MAJOR=0
DEFINES += VERSION_MINOR=2

# baud rates used for uart link in debug port.
# by default the baud rate is slower, but a faster baud rate
# can be enabled with the "fast mode" packet.
DEFINES += UART_BAUD=19200
DEFINES += UART_BAUD_FAST=1000000

# disables uart communication, to save space. if set, flash & eeprom won't be
# programmable until a temporary program is flashed on the MCU to do so.
# DEFINES += DISABLE_COMMS

# size of the display buffer in bytes, must be a multiple of 64.
# DEFINES += DISPLAY_BUFFER_SIZE=2048

# disables auto shutdown for battery overdischarge protection.
# DEFINES += DISABLE_BATTERY_PROTECTION
# changes the default sleep countdown on low battery (if protection enabled).
# DEFINES += POWER_SLEEP_COUNTDOWN=<seconds>

# disables sleep after inactivity period
# DEFINES += DISABLE_INACTIVE_SLEEP
# inactivity sleep and screen dimming countdown timer (if enabled).
# DEFINES += POWER_INACTIVE_COUNTDOWN_SLEEP=<seconds>
# DEFINES += POWER_INACTIVE_COUNTDOWN_DIM=<seconds>

# enables runtime checks (in core/ modules)
# DEFINES += RUNTIME_CHECKS

# disables support for 1-bit or 4-bit images, or only specific encodings, to save space
# DEFINES += GRAPHICS_NO_1BIT_IMAGE
# DEFINES += GRAPHICS_NO_1BIT_RAW_IMAGE
# DEFINES += GRAPHICS_NO_1BIT_MIXED_IMAGE
# DEFINES += GRAPHICS_NO_4BIT_IMAGE
# DEFINES += GRAPHICS_NO_4BIT_RAW_IMAGE
# DEFINES += GRAPHICS_NO_4BIT_MIXED_IMAGE

# disables support for indexed or unindexed images, to save space
# DEFINES += GRAPHICS_NO_INDEXED_IMAGE
# DEFINES += GRAPHICS_NO_UNINDEXED_IMAGE

# disables support for left & right bounded image regions, to save space & improve performance.
# DEFINES += GRAPHICS_NO_HORIZONTAL_IMAGE_REGION

# disables support for transparency in 4-bit images, to save space & improve performance.
# DEFINES += GRAPHICS_NO_TRANSPARENT_IMAGE

# maximum item in a dialog (items are statically allocated)
# DEFINES += DIALOG_MAX_ITEMS