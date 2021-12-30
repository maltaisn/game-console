ifndef TARGET
  $(error No target set. Set target to compile with TARGET=...)
endif

# When changing between simulation and game console compilation, write the value to a file.
# This file is a prerequisite on object files that will force recompilation.
ifeq ($(wildcard .sim),)
  $(shell touch .sim)
endif
ifneq ($(SIM),$(shell cat .sim 2>/dev/null))
  $(shell echo -n $(SIM) > .sim)
endif

ifeq ($(SIM),1)
  include sim.mk
else
  include gc.mk
endif

include $(TARGET)/target.mk
