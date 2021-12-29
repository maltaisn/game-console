# Make definitions for building a game console target.

MCU := atmega3208
F_CPU := 10000000

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
AR := avr-gcc-ar  # not ar, gcc-ar needed for lto to work correctly
                  # https://stackoverflow.com/a/25878408

# ATmega toolchain directory for targeting newer parts.
# It can be downloaded as atpack on Microchip website.
ATMEGA_TOOLCHAIN_DIR := /opt/avr/Atmel.ATmega_DFP

INCLUDE_DIRS += $(ATMEGA_TOOLCHAIN_DIR)/include

DEFINES += F_CPU=$(F_CPU)

CFLAGS += -std=gnu11 -mmcu=$(MCU) -Os \
          -ffunction-sections -fdata-sections -fshort-enums -fpack-struct \
          -B$(ATMEGA_TOOLCHAIN_DIR)/gcc/dev/$(MCU)

# lto somewhat fails for static library, WIP
#CFLAGS += -flto
