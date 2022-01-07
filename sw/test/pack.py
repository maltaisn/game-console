#!/usr/bin/env python3

from packer import Packer, ArrayType

p = Packer(assets_directory="test/assets", padding=4)

p.image("tiger.png")
p.image("tiger-bin.png", index_granularity="128b")

p.sound("music.mid", tempo=120, merge_midi_tracks=True)

p.font("font3x5.png", glyph_width=3, glyph_height=5)
p.font("font5x7.png", glyph_width=5, glyph_height=7)
p.font("font6x9.png", glyph_width=6, glyph_height=9)

p.pack("test/assets.dat")
p.create_header("test/include/assets.h", "test/src/assets.c")

