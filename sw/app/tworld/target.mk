
DEFINES += DIALOG_MAX_ITEMS=4

ALL_TESTS := level

level_test:
	$(MAKE) all TEST_NAME=level TEST_SRC=""
