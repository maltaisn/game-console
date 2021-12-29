
ifndef TARGET
  TARGET := test
endif

.PHONY: clean all sim disasm

all:
	$(MAKE) -C sys all
	$(MAKE) -C core all
	$(MAKE) -C $(TARGET) all

clean:
	$(MAKE) -C sys clean
	$(MAKE) -C sim clean
	$(MAKE) -C core clean
	$(MAKE) -C core clean SIM=1
	$(MAKE) -C $(TARGET) clean
	$(MAKE) -C $(TARGET) clean SIM=1

sim:
	$(MAKE) -C sim all
	$(MAKE) -C core all SIM=1
	$(MAKE) -C $(TARGET) all SIM=1

disasm: all
	$(MAKE) -C $(TARGET) disasm