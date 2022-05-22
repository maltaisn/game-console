
LINKER_SCRIPT := boot.ld

include avr.mk

DEFINES += BOOTLOADER

# Link with the callback vector table symbols
LDFLAGS += -Wl,--just-symbols,boot/callbacks.sym

all: compile

# ==== DEVICE PROGRAMMING ====
# Programming is done via UPDI pin on the debug port, using an USB to serial adapter and pymcuprog.
PROG := pymcuprog
PROG_FUSE := pymcuprog write -m fuse

PROG_MCU = $(MCU)
PROG_PORT := /dev/ttyACM0
PROG_BAUD := 250000

PROG_FLAGS := -t uart -u $(PROG_PORT) -d $(PROG_MCU) -c $(PROG_BAUD)

.PHONY: upload program_fuses

upload: $(MAIN_TARGET).bin
	$(E)pymcuprog erase -m flash $(PROG_FLAGS)
	$(E)pymcuprog write -f $< $(PROG_FLAGS)

program_fuses:
	$(E)$(PROG_FUSE) -o 1 -l 0x04  # enable BOD in active mode only, not sampled, 1.8V
	$(E)$(PROG_FUSE) -o 2 -l 0x82  # calib registers locked, 20 MHz clock
	$(E)$(PROG_FUSE) -o 5 -l 0xe5  # don't erase eeprom on chip erase
	$(E)$(PROG_FUSE) -o 6 -l 0x07  # startup settling time 64 ms
	$(E)$(PROG_FUSE) -o 7 -l 0x00  # app code end (not used)
	$(E)$(PROG_FUSE) -o 8 -l 0x20  # bootloader size (8K)
