ifndef TARGET
  $(error No target set. Set target to compile with TARGET=...)
endif

ifeq ($(SIM),1)
  include sim.mk
else ifeq ($(TEST),1)
  include test.mk
else
  include gc.mk
endif

include $(TARGET)/target.mk
