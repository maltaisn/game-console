# Common base for all projects targeting the game console and linking the core static library.

BUILD_DIR := build/gc

include ../make/game.mk
include ../make/avr.mk
include ../make/upload.mk

LIB_DIRS += ../sys/build ../core/build/gc
LIBS += sys core

BUILD_TARGET := main

CFLAGS += -flto

all: $(MAIN_TARGET).hex size

%.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ $(CFLAGS)

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

disasm: $(MAIN_TARGET).elf
	$(OBJDUMP) -D $< > $(MAIN_TARGET).S

size: $(MAIN_TARGET).elf
	$(OBJDUMP) -Pmem-usage $^
