# Common base for all game projects linking the core static library.

include ../make/common.mk

INCLUDE_DIRS += include

ifdef SIM
  LIB_DIRS += ../core/build/sim
else
  LIB_DIRS += ../core/build/gc
endif
LIBS += core

BUILD_TARGET := main
