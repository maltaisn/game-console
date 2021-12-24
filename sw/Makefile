MCU := atmega3208
F_CPU := 10000000

# firmware version
VERSION_MAJOR := 0
VERSION_MINOR := 1

# baud rate used for uart link in debug port. 500k baud works fine, flash & eeprom writes are
# verified in any case so rare errors should be an issue. Otherwise writing flash is painfully slow.
UART_BAUD := 500000

BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include
BUILD_TARGET := main
MAIN_TARGET := $(BUILD_DIR)/$(BUILD_TARGET)

# ATmega toolchain directory for targeting newer parts.
# It can be downloaded as atpack on Microchip website.
ATMEGA_TOOLCHAIN_DIR := /opt/avr/Atmel.ATmega_DFP

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
AVRDUDE := avrdude

# values defined in the code:
# UART_BAUD=<baud>: the baud rate for UART communication
# VERSION_MAJOR=<version>: major version
# VERSION_MINOR=<version>: minor version
# DISABLE_BAT_PROT: disables auto shutdown for battery overdischarge protection.
DEFINES += UART_BAUD=$(UART_BAUD) VERSION_MAJOR=$(VERSION_MAJOR) VERSION_MINOR=$(VERSION_MINOR)

CFLAGS += -Wall -g -std=gnu11 -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os \
          $(addprefix -D,$(DEFINES)) -I$(INCLUDE_DIR) \
          -ffunction-sections -fdata-sections -fshort-enums -fpack-struct -flto \
          -B$(ATMEGA_TOOLCHAIN_DIR)/gcc/dev/$(MCU) -I$(ATMEGA_TOOLCHAIN_DIR)/include
LDFLAGS = -Wl,--gc-sections -Wl,--print-memory-usage

# programming is done via updi pin in debug port, using updi programmer.
AVRDUDE_MCU = $(MCU)
AVRDUDE_PORT := /dev/ttyUSB0
AVRDUDE_BAUD := 57600
AVRDUDE_PROTOCOL := jtag2updi
AVRDUDE_FLAGS += -v -p $(AVRDUDE_MCU) -P $(AVRDUDE_PORT) -b $(AVRDUDE_BAUD) -c $(AVRDUDE_PROTOCOL)
AVRDUDE_FLASH := -U flash:w:$(MAIN_TARGET).hex

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

CSOURCES := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c)
ASOURCES := $(wildcard $(SRC_DIR)/*.S) $(wildcard $(SRC_DIR)/*/*.S)
OBJECTS := $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.o)) $(addprefix $(BUILD_DIR)/, $(ASOURCES:.S=.o))
DEPS := $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.d)) $(addprefix $(BUILD_DIR)/, $(ASOURCES:.S=.d))

-include $(DEPS)

.PHONY: clean upload

.PRECIOUS: $(BUILD_DIR)/%.o
.PRECIOUS: $(BUILD_DIR)/%.d

all: $(MAIN_TARGET).hex

$(BUILD_DIR)/%.o: %.c
$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

%.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

disasm: $(BUILD_DIR)/$(BUILD_TARGET).elf
	$(OBJDUMP) -D $< > $(BUILD_DIR)/$(BUILD_TARGET).S

size: $(MAIN_TARGET).elf
	$(OBJDUMP) -Pmem-usage $^

upload: $(MAIN_TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_FLASH)

clean:
	rm -rf $(BUILD_DIR)
