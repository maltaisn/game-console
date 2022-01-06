#!/usr/bin/env python3

from packer import Packer, ArrayType

p = Packer(assets_directory="test/assets", padding=4)

p.image("tiger.png")
p.image("tiger-bin.png", index_granularity="128b")

p.sound("music.mid", tempo=120, merge_midi_tracks=True)

p.pack("assets.dat")
p.create_header("include/assets.h", "src/assets.c")

