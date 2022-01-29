.DEFAULT_GOAL := all

include defs.mk

BUILD_DIR ?= build
SRC_DIRS += $(TARGET)/src core
INCLUDE_DIRS += $(TARGET)/include include
LIB_DIRS +=
LIBS +=

BUILD_TARGET := main
MAIN_TARGET = $(BUILD_DIR)/$(BUILD_TARGET)

CFLAGS += -Wall $(addprefix -D,$(DEFINES)) $(addprefix -I,$(INCLUDE_DIRS)) \
          $(addprefix -L,$(LIB_DIRS)) $(addprefix -l,$(LIBS))
CC_FLAGS += -std=gnu11
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

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
ifneq ($(E),)
	@echo $(CC) $<
endif
	$(E)$(CC) -c -o $@ $< $(CFLAGS) $(CC_FLAGS) $(DEPFLAGS)

clean:
	rm -rf $(BUILD_DIR)
