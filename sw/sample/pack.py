#!/usr/bin/env python3

from packer import Packer, ArrayType

tempo = 120

p = Packer(assets_directory="sample/assets", padding=4)

p.image("tiger128.png", name="tiger128_mixed")
p.image("tiger128.png", name="tiger128_raw", raw=True)
p.image("tiger-bin128.png", name="tiger_bin128_mixed")
p.image("tiger-bin128.png", name="tiger_bin128_raw", raw=True)

p.sound("music0.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1, 2})
p.sound("music1.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1})
p.sound("music2.mid", tempo=tempo, merge_midi_tracks=True, channels={0, 1})

p.sound("effect0.mid", "sound", tempo=tempo, channels={2})

p.font("font3x5.png", glyph_width=3, glyph_height=5)
p.font("font5x7.png", glyph_width=5, glyph_height=7)
p.font("font6x9.png", glyph_width=6, glyph_height=9)
p.font("font7x7.png", glyph_width=7, glyph_height=7)

p.image("tiger.png")

str = "Hello world!"
p.raw(str.encode("ascii"), name="text")

p.image("logo-alpha.png")

p.pack("sample/assets.dat")
p.create_header("sample/include/assets.h", "sample/src/assets.c")
