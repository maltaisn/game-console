
SRC_DIRS += sim
BUILD_DIR := build/sim

include common.mk

CC := gcc

LIBS += m glut pthread portaudio png

DEFINES += SIMULATION

CFLAGS += -Wextra -Wno-unused-parameter -std=gnu11 -g3 -O0 -fshort-enums -fpack-struct \
          -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += ${shell pkg-config --cflags glu}
LDFLAGS += ${shell pkg-config --libs glu}

all: $(MAIN_TARGET).elf

%.elf: $(OBJECTS)
ifneq ($(E),)
	@echo Creating ELF file
endif
	$(E)$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
