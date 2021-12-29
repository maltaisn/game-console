.DEFAULT_GOAL := all

# define values present in the code:
# VERSION_MAJOR=<version>: major version
# VERSION_MINOR=<version>: minor version
# UART_BAUD=<baud>: baud rate for normal UART communication
# UART_BAUD_FAST=<baud>: baud rate for fast UART communication
# DISABLE_BAT_PROT: disables auto shutdown for battery overdischarge protection.

# core firmware version
DEFINES += VERSION_MAJOR=0 VERSION_MINOR=2

# baud rates used for uart link in debug port.
# by default the baud rate is slower, but a faster baud rate
# can be enabled with the "fast mode" packet.
DEFINES += UART_BAUD=19200 UART_BAUD_FAST=1000000

BUILD_DIR := build
SRC_DIRS += $(TARGET)/src core
INCLUDE_DIRS += $(TARGET)/include include
LIB_DIRS +=
LIBS +=

BUILD_TARGET := main
MAIN_TARGET = $(BUILD_DIR)/$(BUILD_TARGET)

CFLAGS += -Wall $(addprefix -D,$(DEFINES)) $(addprefix -I,$(INCLUDE_DIRS)) \
          $(addprefix -L,$(LIB_DIRS)) $(addprefix -l,$(LIBS))
LDFLAGS += -Wl,--gc-sections
DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

CSOURCES = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c) $(wildcard $(dir)/*/*.c))
OBJECTS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.o))
DEPS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.d)) \

-include $(DEPS)

.PHONY: clean

.PRECIOUS: $(BUILD_DIR)/%.o
.PRECIOUS: $(BUILD_DIR)/%.d

ifeq ($(V),1)
  E :=
else
  E := @
endif

$(BUILD_DIR)/%.o: %.c .sim
	@mkdir -p $(@D)
ifneq ($(E),)
	@echo CC $<
endif
	$(E)$(CC) -c -o $@ $< $(CFLAGS) $(DEPFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: debug

debug:
	@echo $(OBJECTS)