
DEFINES += DISABLE_INACTIVE_SLEEP

.PHONY: pack

pack:
	$(E)export PYTHONPATH="../utils"; python3 $(TARGET)/pack.py

sample_test:
	$(MAKE) all TEST_NAME=sample TEST_SRC=""
