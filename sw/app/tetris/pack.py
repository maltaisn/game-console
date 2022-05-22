from assets_packer import Packer, ArrayType, Location

tempo = 60

p = Packer(cover_image="cover.png")

# music
with p.group("music"):
    music_args = {"tempo": tempo, "channels": {0, 1}, "merge_midi_tracks": True}
    p.sound("music-theme.mid", name="theme", **music_args)
    p.sound("music-menu.mid", name="menu", **music_args)
    p.sound("music-game-over.mid", name="game_over", **music_args)
    p.sound("music-high-score.mid", name="high_score", **music_args)

# sound
with p.group("sound"):
    sound_args = {"tempo": tempo, "channels": {2}, "merge_midi_tracks": True}
    p.define("tempo", tempo)
    p.sound("sound-hard-drop.mid", name="hard_drop", **sound_args)
    p.sound("sound-hold.mid", name="hold", **sound_args)
    p.sound("sound-combo.mid", name="combo", **sound_args)
    p.sound("sound-perfect.mid", name="perfect", **sound_args)
    p.sound("sound-tspin.mid", name="tspin", **sound_args)

    with p.array("clear", ArrayType.REGULAR):
        for i in range(1, 4+1):
            p.sound(f"sound-clear{i}.mid", name=f"{i}", **sound_args)

# images
with p.group("image"):
    p.image(f"menu.png", raw=False)
    p.image(f"tile-ghost.png", raw=False)

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

p.pack()
