
DEFINES += EEPROM_RESERVED_SPACE=0

ALL_TESTS := graphics

graphics_test:
	$(MAKE) compile TEST_NAME=graphics
