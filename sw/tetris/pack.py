#!/usr/bin/env python3

from packer import Packer, ArrayType

game_version = [0, 1]

tempo = 120

p = Packer(assets_directory="tetris/assets", offset=0x10000)

p.raw(bytes([0xa5, *game_version]), name="header")

p.sound("music.mid", tempo=tempo, channels={0, 1}, merge_midi_tracks=True)

# images
p.image(f"tile-ghost.png")

# fonts
p.font("font5x7.png", "font", name="5x7", glyph_width=5, glyph_height=7)
p.font("font7x7.png", "font", name="7x7", glyph_width=7, glyph_height=7)

p.pack("tetris/assets.dat")
p.create_header("tetris/include/assets.h", "tetris/src/assets.c")

