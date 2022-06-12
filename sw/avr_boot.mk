
LINKER_SCRIPT := boot.ld

include avr.mk

DEFINES += BOOTLOADER

# Link with the callback vector table symbols
LDFLAGS += -Wl,--just-symbols,boot/callbacks.sym

all: compile

# ==== DEVICE PROGRAMMING ====
# Programming is done via UPDI pin on the debug port, using an USB to serial adapter and pymcuprog.
PROG := pymcuprog

PROG_MCU = $(MCU)
PROG_PORT := /dev/ttyACM0
PROG_BAUD := 250000

PROG_FLAGS := -t uart -u $(PROG_PORT) -d $(PROG_MCU) -c $(PROG_BAUD)

.PHONY: upload program_fuses preload

upload: $(MAIN_TARGET).hex
	$(E)pymcuprog erase -m flash $(PROG_FLAGS)
	$(E)pymcuprog write -f $< $(PROG_FLAGS)

preload: clean $(MAIN_TARGET).hex
ifeq ($(PRELOAD),)
	$(error No preload target set. Set target to preload with PRELOAD=...)
endif
	$(MAKE) clean compile TARGET=$(PRELOAD)
	$(E)python3 utils/boot_preload.py $(PRELOAD) $(BUILD_DIR)/preload.hex
	$(E)pymcuprog erase -m flash $(PROG_FLAGS)
	$(E)pymcuprog write -f $(BUILD_DIR)/preload.hex $(PROG_FLAGS)
