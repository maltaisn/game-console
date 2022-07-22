import dat_convert
import tile_gen
import tile_help_gen
from assets_packer import Packer, ArrayType, Location
from tworld import Tile, Entity, Direction, Actor

p = Packer(cover_image="cover.png")
dat_convert.register_builder(p)
tile_gen.register_builder(p)
tile_help_gen.register_builder(p)

# music
tempo = 80
with p.group("music"):
    all_music = ["theme0", "theme1", "menu", "fail", "complete"]
    for music in all_music:
        p.sound(f"music-{music}.mid", name=music, tempo=tempo,
                channels={0, 1}, merge_midi_tracks=True)
    p.define("tempo", tempo)

# sound
with p.group("sound"):
    all_sound = ["timer", "key", "boot", "chip", "lastchip"]
    for sound in all_sound:
        p.sound(f"sound-{sound}.mid", name=sound, tempo=tempo, channels={2})

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

# images
with p.group("image"):
    p.image("arrow-up.png")
    p.image("arrow-down.png")
    p.image("secret-level.png")

    p.image("menu.png")

    p.image("pack/pack-password.png")
    p.image("pack/pack-locked.png")
    with p.array("pack_progress", ArrayType.REGULAR):
        for i in range(9):
            p.image(f"pack/pack-progress-{i}.png")

# tileset
with p.group("tileset"):
    with p.array("bottom", ArrayType.REGULAR):
        bottom_map = p.tileset("tileset-bottom-{}.png", name=f"bottom{i}",
                               width=9, height=8, tile_width=14, variants=2)

    with p.array("top", ArrayType.REGULAR):
        top_map = p.tileset("tileset-top.png", name="top", width=8, height=8,
                            tile_width=12, alpha=True)

    with p.group("map", Location.INTERNAL):
        p.define("bottom_size", len(bottom_map))
        p.raw(bottom_map, name="bottom")
        p.raw(top_map, name="top")

# death messages
with p.array("end_cause", ArrayType.INDEXED_ABS):
    messages = [
        "Ooops! Chip can't swim without flippers!",
        "Ooops! Don't step in the fire without fire boots!",
        "Ooops! Don't touch the bombs!",
        "Ooops! Look out for creatures!",
        "Ooops! Watch out for moving blocks!",
        "Ooops!\nOut of time!",
    ]
    for i, message in enumerate(messages):
        p.string(message, name=f"{i}")

# tile help (separated by page)
help_map = {
    Actor(Entity.CHIP, Direction.SOUTH): "Chip",
    Actor(Entity.BUG, Direction.EAST): "Bug",
    Actor(Entity.PARAMECIUM, Direction.EAST): "Paramecium",
    Actor(Entity.GLIDER, Direction.EAST): "Glider",
    # ----
    Actor(Entity.FIREBALL, Direction.NORTH): "Fireball",
    Actor(Entity.BALL, Direction.NORTH): "Ball",
    Actor(Entity.BLOB, Direction.NORTH): "Blob",
    Actor(Entity.TANK, Direction.EAST): "Tank",
    # ----
    Actor(Entity.WALKER, Direction.EAST): "Walker",
    Actor(Entity.TEETH, Direction.EAST): "Teeth",
    Tile.BLOCK: "Block",
    Tile.FLOOR: "Floor",
    # ----
    Tile.ICE: "Ice",
    Tile.WATER: "Water",
    Tile.FIRE: "Fire",
    Tile.BOMB: "Bomb",
    # ----
    Tile.WALL: "Wall",
    Tile.RECESSED_WALL: "Recessed wall",
    Tile.WALL_BLUE_REAL: "Blue wall",
    Tile.LOCK_BLUE: "Lock",
    # ----
    Tile.KEY_YELLOW: "Yellow key",
    Tile.KEY_BLUE: "Blue key",
    Tile.KEY_RED: "Red key",
    Tile.KEY_GREEN: "Green key",
    # ----
    Tile.BOOTS_WATER: "Flippers",
    Tile.BOOTS_FIRE: "Fire boots",
    Tile.BOOTS_ICE: "Skates",
    Tile.BOOTS_FORCE_FLOOR: "Suction boots",
    # ----
    Tile.BUTTON_GREEN: "Toggle button",
    Tile.BUTTON_RED: "Cloner button",
    Tile.BUTTON_BROWN: "Trap button",
    Tile.BUTTON_BLUE: "Tank button",
    # ----
    Tile.THIN_WALL_SE: "Thin wall",
    Tile.ICE_CORNER_NW: "Ice corner",
    Tile.TOGGLE_WALL: "Toggle wall",
    Tile.TOGGLE_FLOOR: "Toggle floor",
    # ----
    Tile.DIRT: "Dirt",
    Tile.GRAVEL: "Gravel",
    Tile.FORCE_FLOOR_E: "Force floor",
    Tile.FORCE_FLOOR_RANDOM: "Random floor",
    # ----
    Tile.HINT: "Hint",
    Tile.CHIP: "Computer chip",
    Tile.SOCKET: "Socket",
    Tile.EXIT: "Exit",
    # ----
    Tile.THIEF: "Thief",
    Tile.TRAP: "Trap",
    Tile.CLONER: "Clone machine",
    Tile.TELEPORTER: "Teleporter",
}
with p.array("help", ArrayType.INDEXED_ABS_FLASH):
    for i, entry in enumerate(help_map.items()):
        tile, name = entry
        p.tile_help(tile, name, name=f"{i}")

# levels
with p.array("level_packs", ArrayType.INDEXED_ABS):
    p.level_pack("cclp1.dat", "CCLP1")
    p.level_pack("cclp2.dat", "CCLP2")
    p.level_pack("cclp3.dat", "CCLP3")
    p.level_pack("cclp4.dat", "CCLP4")

p.pack()
