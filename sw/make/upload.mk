# Definitions for upload firmware to game console with avrdude.
# Programming is done via UPDI pin on the debug port, and using an UPDI programmer (jtag2updi).

AVRDUDE := avrdude

AVRDUDE_MCU = $(MCU)
AVRDUDE_PORT := /dev/ttyUSB0
AVRDUDE_BAUD := 57600
AVRDUDE_PROTOCOL := jtag2updi
AVRDUDE_FLAGS += -v -p $(AVRDUDE_MCU) -P $(AVRDUDE_PORT) -b $(AVRDUDE_BAUD) -c $(AVRDUDE_PROTOCOL)
AVRDUDE_FLASH := -U flash:w:$(MAIN_TARGET).hex

.PHONY: upload

upload: $(MAIN_TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_FLASH)
