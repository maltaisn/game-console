
DEFINES += DIALOG_MAX_ITEMS=6

.PHONY: pack

pack:
	$(E)export PYTHONPATH="../utils"; python3 $(TARGET)/pack.py
