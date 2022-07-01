
DEFINES += DIALOG_MAX_ITEMS=5

#DEFINES += FPS_MONITOR

ALL_TESTS := level

level_test: assets
	$(MAKE) compile TEST_NAME=level
