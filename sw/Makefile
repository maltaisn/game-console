ifndef TARGET
  $(error No target set. Set target to compile with TARGET=...)
endif

ifeq ($(SIM),1)
  include sim.mk
else
  include gc.mk
endif

include $(TARGET)/target.mk
