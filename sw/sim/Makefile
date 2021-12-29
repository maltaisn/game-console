# Makefile for building the simulator static library.

include ../make/common.mk
include ../make/sim.mk

BUILD_TARGET := libsim

INCLUDE_DIRS += ../core/include

all: $(MAIN_TARGET).a

%.a: $(OBJECTS)
	$(AR) rcs $@ $^
