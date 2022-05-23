#!/usr/bin/env python3

from assets_packer import Packer, Location

p = Packer(cover_image="cover.png")

# except for the cover, all assets used by the system app are stored in
# internal program memory. The reason for this is that the system app is
# often bundled with the bootloader to allow programming, and at that
# point the flash memory isn't programmed yet.
p.location = Location.INTERNAL

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

# images
with p.group("image"):
    p.image("arrow-up.png")
    p.image("arrow-down.png")

p.pack()
