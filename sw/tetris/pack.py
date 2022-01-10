#!/usr/bin/env python3

from packer import Packer, ArrayType

tempo = 120

p = Packer(assets_directory="tetris/assets")

# images
p.set_array_options("tile", ArrayType.REGULAR)
for piece in ["i", "j", "l", "o", "s", "t", "z", "ghost"]:
    p.image(f"tile-{piece}.png", "tile")

# fonts
p.font("font5x7.png", "font", name="5x7", glyph_width=5, glyph_height=7)
p.font("font7x7.png", "font", name="7x7", glyph_width=7, glyph_height=7)

p.pack("tetris/assets.dat")
p.create_header("tetris/include/assets.h", "tetris/src/assets.c")

