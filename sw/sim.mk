
SRC_DIRS += sim
BUILD_DIR := build/sim

include common.mk

CC := gcc

LIBS += glut pthread portaudio m

DEFINES += SIMULATION

CFLAGS += -std=gnu11 -g3 -O0 -fshort-enums -fpack-struct
CFLAGS += ${shell pkg-config --cflags glu}
LDFLAGS += ${shell pkg-config --libs glu}

all: $(MAIN_TARGET).elf

%.elf: $(OBJECTS)
ifneq ($(E),)
	@echo Creating ELF file
endif
	$(E)$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
