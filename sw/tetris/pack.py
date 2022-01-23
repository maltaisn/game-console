#!/usr/bin/env python3

from packer import Packer, ArrayType

version_major = 0
version_minor = 5

tempo = 120

p = Packer(assets_directory="tetris/assets", offset=0x10000)

p.raw(bytes([0xa5, version_major, version_minor]), name="header")

# defines
p.define("game_version_major", version_major)
p.define("game_version_minor", version_minor)
p.define("asset_sound_tempo", tempo)

# music
music_args = {
    "group": "music",
    "tempo": tempo,
    "channels": {0, 1},
    "merge_midi_tracks": True,
}
p.sound("music-theme.mid", name="theme", **music_args)
p.sound("music-menu.mid", name="menu", **music_args)
p.sound("music-game-over.mid", name="game-over", **music_args)
p.sound("music-high-score.mid", name="high-score", **music_args)

# sound
sound_args = {
    "tempo": tempo,
    "channels": {2},
    "merge_midi_tracks": True,
}
p.sound("sound-hard-drop.mid", name="hard-drop", group="sound", **sound_args)
p.sound("sound-hold.mid", name="hold", group="sound", **sound_args)
p.sound("sound-combo.mid", name="combo", group="sound", **sound_args)
p.sound("sound-perfect.mid", name="perfect", group="sound", **sound_args)
p.sound("sound-tspin.mid", name="tspin", group="sound", **sound_args)

p.set_array_options("sound_clear", ArrayType.REGULAR)
for i in range(1, 4+1):
    p.sound(f"sound-clear{i}.mid", name=f"{i}", group="sound_clear", **sound_args)

# images
p.image(f"menu.png")
p.image(f"tile-ghost.png")

# fonts
p.font("font5x7.png", "font", name="5x7", glyph_width=5, glyph_height=7)
p.font("font7x7.png", "font", name="7x7", glyph_width=7, glyph_height=7)

p.pack("tetris/assets.dat")
p.create_header("tetris/include/assets.h", "tetris/src/assets.c")
