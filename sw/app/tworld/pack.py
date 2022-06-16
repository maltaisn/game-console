from assets_packer import Packer

p = Packer(cover_image="cover.png")

# music
tempo = 60
with p.group("music"):
    music_args = {"tempo": tempo, "channels": {0, 1}, "merge_midi_tracks": True}
    # p.sound("music0.mid", name="theme0", **music_args)
    # p.sound("music1.mid", name="theme1", **music_args)
    p.define("tempo", tempo)

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

p.pack()
