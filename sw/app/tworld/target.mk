
DEFINES += DIALOG_MAX_ITEMS=5

ALL_TESTS := level

level_test:
	$(MAKE) all TEST_NAME=level TEST_SRC=""
