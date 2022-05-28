
# This file is mostly copied from sim.mk, with some modifications
# to integrate Google Test and to compile C++ code.

PLATFORM := test

include common.mk

CSOURCES += app/callbacks.c
SRC_DIRS += sim
INCLUDE_DIRS += boot/include
LIBS += m z pthread png

# Google Test
GTEST_DIR := gtest
GTEST_DIR_LIB := $(GTEST_DIR)/build/lib
INCLUDE_DIRS += $(GTEST_DIR)/googletest/include
LIBS += gtest gtest_main
LIB_DIRS += $(GTEST_DIR_LIB)

ALL_TESTS =  # to be set in target.mk
TEST_SRC_DIR := $(TARGET)/test
TEST_NAME_P = $(TEST_NAME)_test

CC := gcc
CXX := g++

# tests run in "headless" simulator mode.
# the simulator will have no GUI, produce no sound, and time will be controllable.
DEFINES += SIMULATION SIMULATION_HEADLESS BOOTLOADER TESTING

CFLAGS += -Wno-unused-parameter -g3 -O0 \
          -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -pthread

CC_FLAGS += -fshort-enums -fpack-struct
CXX_FLAGS += -std=c++17

CXXSOURCES := $(TEST_SRC_DIR)/$(TEST_NAME_P).cpp
OBJECTS += $(addprefix $(BUILD_DIR)/, $(CXXSOURCES:.cpp=.o))

DEPS = $(addprefix $(BUILD_DIR)/, $(CPPSOURCES:.cpp=.d)) \

.PHONY: all test

$(BUILD_DIR)/%.o: %.cpp $(TARGET_CONFIG_FILE)
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
	$(E)mkdir $(GTEST_DIR)/build/; cd $(GTEST_DIR)/build/; cmake ..; make;

compile: $(BUILD_DIR)/$(TEST_NAME_P)

all:
	$(error Use the 'test' target instead to run all tests)

test:
	$(MAKE) $(addsuffix _test, $(ALL_TESTS))
	cd $(TARGET); $(foreach t,$(ALL_TESTS),build/test/$(t)_test;)
