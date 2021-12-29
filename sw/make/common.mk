# Common Make definitions for all projects.

.DEFAULT_GOAL := all

ifndef BUILD_DIR
  BUILD_DIR := build
endif

SRC_DIR := src
INCLUDE_DIRS += ../include
LIB_DIRS +=
LIBS +=

MAIN_TARGET = $(BUILD_DIR)/$(BUILD_TARGET)

DEFINES +=
CFLAGS += -Wall $(addprefix -D,$(DEFINES)) $(addprefix -I,$(INCLUDE_DIRS)) \
          $(addprefix -L,$(LIB_DIRS)) $(addprefix -l,$(LIBS))
LDFLAGS += -Wl,--gc-sections

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

CSOURCES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c)
OBJECTS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.o))
DEPS = $(addprefix $(BUILD_DIR)/, $(CSOURCES:.c=.d))

-include $(DEPS)

.PHONY: clean

.PRECIOUS: $(BUILD_DIR)/%.o
.PRECIOUS: $(BUILD_DIR)/%.d

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(DEPFLAGS)

clean:
	rm -rf $(BUILD_DIR)
