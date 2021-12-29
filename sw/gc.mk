
MCU := atmega3208
F_CPU := 10000000

SRC_DIRS += sys

include common.mk

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump

# ATmega toolchain directory for targeting newer parts.
# It can be downloaded as atpack on Microchip website.
ATMEGA_TOOLCHAIN_DIR := /opt/avr/Atmel.ATmega_DFP
INCLUDE_DIRS += $(ATMEGA_TOOLCHAIN_DIR)/include

DEFINES += F_CPU=$(F_CPU)

CFLAGS += -std=gnu11 -mmcu=$(MCU) -Os \
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
	$(E)$(OBJDUMP) -Pmem-usage $^

# ==== UPLOADING ====
# Programming is done via UPDI pin on the debug port, and using an UPDI programmer (jtag2updi).
AVRDUDE := avrdude

AVRDUDE_MCU = $(MCU)
AVRDUDE_PORT := /dev/ttyUSB0
AVRDUDE_BAUD := 57600
AVRDUDE_PROTOCOL := jtag2updi
AVRDUDE_FLAGS += -v -p $(AVRDUDE_MCU) -P $(AVRDUDE_PORT) -b $(AVRDUDE_BAUD) -c $(AVRDUDE_PROTOCOL)
AVRDUDE_FLASH := -U flash:w:$(MAIN_TARGET).hex

.PHONY: upload

upload: $(MAIN_TARGET).hex
	$(E)$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_FLASH)
