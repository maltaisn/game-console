
LINKER_SCRIPT := app.ld

# This files implements the callback vector table.
CSOURCES += app/callbacks.c

include avr.mk

# Link with the boot symbols to allow symbol reuse
# Bootloader must have been compiled before compiling app, so there's a check for that.
LDFLAGS += -Wl,--just-symbols,$(BOOT_SYMBOLS_FILE)

.PHONY: sym_check pack_app

sym_check:
ifeq ($(wildcard $(BOOT_SYMBOLS_FILE)),)
    $(error Could not find bootloader symbols. Bootloader should be built first)
endif

compile: sym_check $(MAIN_TARGET).hex size

all: assets compile
	$(E)python3 utils/app_packer.py $(TARGET)
