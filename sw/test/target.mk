
GAME_VERSION_MAJOR := 0
GAME_VERSION_MINOR := 0

DEFINES += GAME_VERSION_MAJOR=$(GAME_VERSION_MAJOR) GAME_VERSION_MINOR=$(GAME_VERSION_MINOR)

.PHONY: pack

pack:
	$(E)export PYTHONPATH="../utils"; python3 $(TARGET)/pack.py
