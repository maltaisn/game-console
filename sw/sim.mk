
PLATFORM := sim

include common.mk

CSOURCES += app/callbacks.c
SRC_DIRS += sim
INCLUDE_DIRS += boot/include

ifneq ($(TARGET),boot)
SRC_DIRS += boot/src
endif

CC := gcc

LIBS += m glut pthread portaudio png

DEFINES += SIMULATION BOOTLOADER

CFLAGS += -Wno-unused-parameter -std=gnu11 -g3 -O0 -fshort-enums -fpack-struct \
          -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += ${shell pkg-config --cflags glu}
LDFLAGS += ${shell pkg-config --libs glu}

compile: $(MAIN_TARGET)

all: compile

$(MAIN_TARGET): $(OBJECTS)
ifneq ($(E),)
	@echo Creating binary file
endif
	$(E)$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
