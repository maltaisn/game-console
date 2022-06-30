import dat_convert
import tile_gen
from assets_packer import Packer, ArrayType, Location

p = Packer(cover_image="cover.png")
dat_convert.register_builder(p)
tile_gen.register_builder(p)

# music
tempo = 80
with p.group("music"):
    all_music = ["theme0", "theme1", "menu", "fail", "complete"]
    for music in all_music:
        p.sound(f"music-{music}.mid", name=music, tempo=tempo,
                channels={0, 1}, merge_midi_tracks=True)
    p.define("tempo", tempo)

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

# images
with p.group("image"):
    p.image("arrow-up.png")
    p.image("arrow-down.png")

    p.image("menu.png")

    p.image("pack/pack-password.png")
    p.image("pack/pack-locked.png")
    with p.array("pack_progress", ArrayType.REGULAR):
        for i in range(9):
            p.image(f"pack/pack-progress-{i}.png")

# tileset
with p.group("tileset"):
    with p.array("bottom", ArrayType.REGULAR):
        bottom_map = p.tileset("tileset-bottom.png", name="bottom", width=8,
                               tile_width=14, height=8)

    with p.array("top", ArrayType.REGULAR):
        top_map = p.tileset("tileset-top.png", name="top", width=8, height=8,
                            tile_width=12, alpha=True)

    with p.group("map", Location.INTERNAL):
        p.raw(bottom_map, name="bottom")
        p.raw(top_map, name="top")

# death messages
with p.array("end_cause", ArrayType.INDEXED_ABS):
    messages = [
        "Ooops! Don't step in the fire without fire boots!",
        "Ooops! Look out for creatures!",
        "Ooops! Watch out for moving blocks!",
        "Ooops! Chip can't swim without flippers!",
        "Ooops! Don't touch the bombs!",
        "Ooops!\nOut of time!",
    ]
    for i, message in enumerate(messages):
        p.string(message, name=f"{i}")

# levels
with p.array("level_packs", ArrayType.INDEXED_ABS):
    p.level_pack("cclp1.dat", "CCLP1")
    p.level_pack("cclp2.dat", "CCLP2")
    p.level_pack("cclp3.dat", "CCLP3")
    p.level_pack("cclp4.dat", "CCLP4")

p.pack()
