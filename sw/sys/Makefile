# Makefile for building the core static library.

include ../make/common.mk
include ../make/avr.mk

# baud rates used for uart link in debug port.
# by default the baud rate is slower, but a faster baud rate
# can be enabled with the "fast mode" packet.
UART_BAUD := 19200
UART_BAUD_FAST := 1000000

BUILD_TARGET := libsys

# values defined in the code:
# UART_BAUD=<baud>: baud rate for normal UART communication
# UART_BAUD_FAST=<baud>: baud rate for fast UART communication
# DISABLE_BAT_PROT: disables auto shutdown for battery overdischarge protection.
DEFINES += UART_BAUD=$(UART_BAUD) UART_BAUD_FAST=$(UART_BAUD_FAST)

CFLAGS +=
LDFLAGS +=

all: $(MAIN_TARGET).a

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

disasm: $(MAIN_TARGET).a
	$(OBJDUMP) -D $< > $(MAIN_TARGET).S
