
PLATFORM := avr

include common.mk
include toolchain.mk

# MCU definitions
MCU := atmega3208
F_CPU := 10000000

SRC_DIRS += sys

# AVR compilation
CC := $(AVR_TOOLCHAIN_DIR)avr-gcc
OBJCOPY := $(AVR_TOOLCHAIN_DIR)avr-objcopy
OBJDUMP := $(AVR_TOOLCHAIN_DIR)avr-objdump
NM := $(AVR_TOOLCHAIN_DIR)avr-nm
SIZE := utils/avr_size.py

INCLUDE_DIRS += $(ATMEGA_DFP_DIR)include

MAP_FILE := $(MAIN_TARGET).map
BOOT_SYMBOLS_FILE := boot/build/boot.sym

DEFINES += F_CPU=$(F_CPU)

CFLAGS += -mmcu=$(MCU) -Os -mcall-prologues \
          -ffunction-sections -fdata-sections -fshort-enums -fpack-struct -flto \
          -B$(ATMEGA_DFP_DIR)gcc/dev/$(MCU) -Wl,-T,$(LINKER_SCRIPT)      \
          -Wl,-Map=$(MAP_FILE) -Wl,--defsym=DISPLAY_PAGE_HEIGHT=$(display_page_height)

OBJECTS += $(addprefix $(BUILD_DIR)/, $(ASOURCES:.S=.o))
DEPS += $(addprefix $(BUILD_DIR)/, $(CSOURCES:.S=.d))

compile: $(MAIN_TARGET).hex $(BOOT_SYMBOLS_FILE) size

disasm: $(MAIN_TARGET).S

$(BUILD_DIR)/%.o: %.S $(TARGET_CONFIG_FILE)
	@mkdir -p $(@D)
ifneq ($(E),)
	@echo gcc $<
endif
	$(E)$(CC) -c -o $@ $< $(CFLAGS) $(CC_FLAGS) $(DEPFLAGS)

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

$(MAIN_TARGET).S: $(MAIN_TARGET).elf
ifneq ($(E),)
	@echo Disassembling ELF file
endif
	$(E)$(OBJDUMP) -d $< > $@

size: $(MAIN_TARGET).elf
	$(E)$(SIZE) $(MAP_FILE)

$(BOOT_SYMBOLS_FILE): $(MAIN_TARGET).elf
# for bootloader, get symbols from binary and write a symbols file from them.
# - Symbol suffixes like .constprop or .ltopriv are removed since it prevents the app from using it.
#   This should be fine as long as they had the same calling convention as the original (it should).
# - Symbols startings with '_' are discarded (defined by linker script or gcc internals).
ifeq ($(TARGET),boot)
ifneq ($(E),)
	@echo Exporting boot symbols
endif
	$(E)$(NM) $< | python3 utils/boot_sym_filter.py > $(BOOT_SYMBOLS_FILE)
endif
