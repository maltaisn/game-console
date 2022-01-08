#!/usr/bin/env python3

from packer import Packer, ArrayType

tempo = 120

p = Packer(assets_directory="tetris/assets", padding=4)

# images
# TODO

# fonts
p.font("font5x7.png", "font", name="5x7", glyph_width=5, glyph_height=7)
p.font("font7x7.png", "font", name="7x7", glyph_width=7, glyph_height=7)

p.pack("tetris/assets.dat")
p.create_header("tetris/include/assets.h", "tetris/src/assets.c")

