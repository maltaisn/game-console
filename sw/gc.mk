
MCU := atmega3208
F_CPU := 10000000

SRC_DIRS += sys
BUILD_DIR := build/gc

include common.mk

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
SIZE := avr-size

# ATmega toolchain directory for targeting newer parts.
# It can be downloaded as atpack on Microchip website.
ATMEGA_TOOLCHAIN_DIR := /opt/avr/Atmel.ATmega_DFP
INCLUDE_DIRS += $(ATMEGA_TOOLCHAIN_DIR)/include

DEFINES += F_CPU=$(F_CPU)

CFLAGS += -mmcu=$(MCU) -Os \
          -ffunction-sections -fdata-sections -fshort-enums -fpack-struct -flto \
          -B$(ATMEGA_TOOLCHAIN_DIR)/gcc/dev/$(MCU)

all: $(MAIN_TARGET).hex size

%.elf: $(OBJECTS)
ifneq ($(E),)
	@echo Creating ELF file
endif
	$(E)$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%.hex: %.elf
ifneq ($(E),)
	@echo Creating HEX file
endif
	$(E)$(OBJCOPY) -O ihex $< $@

disasm: $(MAIN_TARGET).elf
ifneq ($(E),)
	@echo Disassembling ELF file
endif
	$(E)$(OBJDUMP) -D $< > $(MAIN_TARGET).S

size: $(MAIN_TARGET).elf
	$(E)$(SIZE) --mcu=$(MCU) -B $^

# ==== UPLOADING ====
# Programming is done via UPDI pin on the debug port, and using a serial adapter and pymcuprog.
# The serial adapter has 2 interfaces, one for programming, and one for communication with MCU.
# The communication interface is used to program flash & EEPROM among other things, using gcprog.
PROG := pymcuprog

PROG_MCU = $(MCU)
PROG_PORT := /dev/ttyACM0
PROG_BAUD := 250000

PROG_FLAGS := -t uart -u $(PROG_PORT) -d $(PROG_MCU) -c $(PROG_BAUD)

.PHONY: upload

upload: $(MAIN_TARGET).hex
	$(E)pymcuprog erase $(PROG_FLAGS)
	$(E)pymcuprog write -f $(MAIN_TARGET).hex $(PROG_FLAGS)
