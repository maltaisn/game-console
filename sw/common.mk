.DEFAULT_GOAL := all

# Common definitions. These might be extended or overriden in other make files.
BUILD_DIR := $(TARGET)/build/$(PLATFORM)
SRC_DIRS += $(TARGET)/src core
INCLUDE_DIRS += $(TARGET)/include include
LIB_DIRS +=
LIBS +=

# Packaging
TARGET_CONFIG_FILENAME := target.cfg
TARGET_CONFIG_FILE := $(TARGET)/target.cfg
ASSETS_FILE := $(TARGET)/assets.dat
APP_PACK_FILE := $(TARGET)/target.app

# Boot configuration file
include boot/$(TARGET_CONFIG_FILENAME)
DEFINES += BOOT_VERSION=$(boot_version)

# App configuration file
ifneq ($(wildcard $(TARGET_CONFIG_FILE)),)
  eeprom_space := 0
  include $(TARGET_CONFIG_FILE)
  DEFINES += DISPLAY_PAGE_HEIGHT=$(display_page_height) APP_ID=$(id) APP_VERSION=$(version) \
             EEPROM_RESERVED_SPACE=$(eeprom_space)
endif

# Compilation
BUILD_TARGET := main
MAIN_TARGET = $(BUILD_DIR)/$(BUILD_TARGET)

CFLAGS += -Wall -Wno-attributes $(addprefix -D,$(DEFINES)) $(addprefix -I,$(INCLUDE_DIRS)) \
          $(addprefix -L,$(LIB_DIRS)) $(addprefix -l,$(LIBS))
CC_FLAGS += -std=gnu11
LDFLAGS += -Wl,--gc-sections
DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

CSOURCES += $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c) $(wildcard $(dir)/*/*.c))
OBJECTS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.o))
DEPS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.d))

-include $(DEPS)

.PHONY: clean

.PRECIOUS: $(BUILD_DIR)/%.o
.PRECIOUS: $(BUILD_DIR)/%.d

ifeq ($(V),1)
  E :=
else
  E := @
endif

$(BUILD_DIR)/%.o: %.c $(TARGET_CONFIG_FILE)
	@mkdir -p $(@D)
ifneq ($(E),)
	@echo gcc $<
endif
	$(E)$(CC) -c -o $@ $< $(CFLAGS) $(CC_FLAGS) $(DEPFLAGS)

clean:
	$(E)rm -rf $(BUILD_DIR)
	$(E)rm -f $(ASSETS_FILE)
	$(E)rm -f $(APP_PACK_FILE)

# Assets packing

ifneq ($(TARGET),boot)
$(ASSETS_FILE): $(TARGET)/pack.py $(TARGET)/assets/*
	$(E)export PYTHONPATH="$(PWD)/utils"; cd $(TARGET); python3 pack.py

assets: $(ASSETS_FILE)
endif