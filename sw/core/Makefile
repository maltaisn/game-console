# Makefile for building the core static library.

ifdef SIM
  BUILD_DIR := build/sim
else
  BUILD_DIR := build/gc
endif

include ../make/common.mk
ifdef SIM
  include ../make/sim.mk
else
  include ../make/avr.mk
endif

# core firmware version
VERSION_MAJOR := 0
VERSION_MINOR := 2

BUILD_TARGET := libcore

# values defined in the code:
# VERSION_MAJOR=<version>: major version
# VERSION_MINOR=<version>: minor version
DEFINES += VERSION_MAJOR=$(VERSION_MAJOR) VERSION_MINOR=$(VERSION_MINOR)

all: $(MAIN_TARGET).a

%.a: $(OBJECTS)
	$(AR) rcs $@ $^

ifndef SIM
disasm: $(MAIN_TARGET).a
	$(OBJDUMP) -D $< > $(MAIN_TARGET).S
endif