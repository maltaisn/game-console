#!/usr/bin/env python3

from packer import Packer, ArrayType

tempo = 120

p = Packer(assets_directory="test/assets", padding=4)

p.image("tiger128.png")

p.sound("music0.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1, 2})
p.sound("music1.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1})
p.sound("music2.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1})

p.sound("effect0.mid", "sound", tempo=tempo, channels={2})

p.font("font3x5.png", glyph_width=3, glyph_height=5)
p.font("font5x7.png", glyph_width=5, glyph_height=7)
p.font("font6x9.png", glyph_width=6, glyph_height=9)
p.font("font7x7.png", glyph_width=7, glyph_height=7)

p.image("tiger.png")
p.image("tiger-bin.png")

str = "Hello world!"
p.raw(str.encode("ascii"), name="text")

p.pack("test/assets.dat")
p.create_header("test/include/assets.h", "test/src/assets.c")
