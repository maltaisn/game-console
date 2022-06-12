
PLATFORM := sim

include common.mk

SRC_DIRS += sim
INCLUDE_DIRS += boot/include

# Include the bootloader source since this is also the entry point for the simulator.
# If target is boot, don't include its source twice.
ifneq ($(TARGET),boot)
SRC_DIRS += boot/src
endif

CC := gcc

LIBS += m glut pthread portaudio png

DEFINES += SIMULATION BOOTLOADER

CFLAGS += -Wno-unused-parameter -g3 -O0 -fshort-enums -fpack-struct \
          -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += ${shell pkg-config --cflags glu}
LDFLAGS += ${shell pkg-config --libs glu}

compile: $(MAIN_TARGET)

all: assets compile

$(MAIN_TARGET): $(OBJECTS)
ifneq ($(E),)
	@echo Creating binary file
endif
	$(E)$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
