# Common base for all projects targeting the simulator and linking the simulator library.

BUILD_DIR := build/sim

include ../make/game.mk
include ../make/sim.mk

LIB_DIRS += ../sim/build
LIBS += sim

BUILD_TARGET := main

all: $(MAIN_TARGET).elf

%.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ $(CFLAGS)
