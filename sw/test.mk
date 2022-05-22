
PLATFORM := test

include common.mk

CSOURCES += app/callbacks.c
SRC_DIRS += sim
INCLUDE_DIRS += boot/include

GTEST_DIR := gtest
GTEST_DIR_LIB := $(GTEST_DIR)/build/lib
TEST_SRC_DIR := $(TARGET)/test

TEST_NAME_P = $(TEST_NAME)_test

LIBS += m z pthread png gtest gtest_main
LIB_DIRS += $(GTEST_DIR_LIB)
INCLUDE_DIRS += $(GTEST_DIR)/googletest/include

CC := gcc
CXX := g++

# tests run in "headless" simulator mode.
# the simulator will have no GUI, produce no sound, and time will be controllable.
DEFINES += SIMULATION SIMULATION_HEADLESS BOOTLOADER

CFLAGS += -Wextra -Wno-unused-parameter -g3 \
          -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -pthread

CC_FLAGS += -fshort-enums -fpack-struct -O0
CXX_FLAGS += -std=c++17 -O0

CXXSOURCES := $(TEST_SRC_DIR)/$(TEST_NAME_P).cpp
OBJECTS += $(addprefix $(BUILD_DIR)/, $(CXXSOURCES:.cpp=.o))

DEPS = $(addprefix $(BUILD_DIR)/, $(CPPSOURCES:.cpp=.d)) \

.PHONY: test

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
ifneq ($(E),)
	@echo $(CXX) $<
endif
	$(E)$(CXX) -c -o $@ $< $(CFLAGS) $(CXX_FLAGS) $(DEPFLAGS)

$(BUILD_DIR)/$(TEST_NAME_P): $(OBJECTS) $(GTEST_DIR_LIB)
ifneq ($(E),)
	@echo Creating binary file
endif
	$(E)$(CXX) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

$(GTEST_DIR_LIB):
	mkdir $(GTEST_DIR)/build/; cd $(GTEST_DIR)/build/; cmake ..; make;

all: $(BUILD_DIR)/$(TEST_NAME_P)
