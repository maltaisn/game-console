#!/usr/bin/env python3
import argparse
import re
import sys
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
from typing import List, Optional, Union, Dict, Tuple, Iterable

import numpy

import lzss
from tworld import TileWorld, Tile, Level, Actor, Entity, Direction

sys.path.append(str(Path(__file__).absolute().parent / "../../../utils"))  # for standalone run
from assets.types import PackResult, DataObject, PackError

# LEVEL DATA FORMAT
# The data format for a level pack is described below. All multi-byte fields are little-endian.
#
# - [0..1]: signature, 0x5754 ('TW')
# - [2]: number of levels (=N)
# - [3..(N*2)]: level index, with each entry being offset from previous level position.
#     the first offset is from the start of level pack data
# - level pack name: zero terminated string (max size 12 including terminator).
# - level data:
# - [0..1]: time limit in seconds. 0 for unlimited duration (max 999)
# - [2..3]: required number of chips
# - [4..5]: layer data size in bytes
# - [6..9]: 4 letters password (only A-Z allowed)
# - [10..17]: index from the start position of level data to various chunks, in order:
#     - [10..11]: title
#     - [12..13]: hint (0 if none)
#     - [14..15]: trap-button linkage
#     - [16..17]: cloner-button linkage
# - [18..]: LZSS-compressed layer data. Uncompressed layout is:
#     - [0..767]: bottom layer, 6-bit per tile
#     - [768..1535]: top layer, 6-bit per tile
# - title: zero terminated string, max 40 chars
# - hint: zero terminated string, max 128 chars
# - linkage (trap & cloner):
#     - [0]: number of links (max 32)
#     - [1..]: a list of chunks with the following format:
#         - [0]: button x
#         - [1]: button y
#         - [2]: trap/cloner x
#         - [3]: trap/cloner y

# All tiles are re-encoded in tworld from MS DAT IDs.
# Not all tiles can appear on bottom and top layers.
# Block cloner is a special case since it results in cloner (bottom) + directional block (top)
# and thus is present is both layer remap dictionnaries.
# There are also a few tiles that don't exist in the original game (e.g: static tiles)
TILE_REMAP_BOTTOM = {
    0x00: Tile.FLOOR,
    0x2b: Tile.TRAP,
    0x26: Tile.TOGGLE_FLOOR,
    0x25: Tile.TOGGLE_WALL,
    0x23: Tile.BUTTON_GREEN,
    0x24: Tile.BUTTON_RED,
    0x27: Tile.BUTTON_BROWN,
    0x28: Tile.BUTTON_BLUE,
    0x64: Tile.KEY_BLUE,
    0x65: Tile.KEY_RED,
    0x06: Tile.THIN_WALL_N,
    0x07: Tile.THIN_WALL_W,
    0x08: Tile.THIN_WALL_S,
    0x09: Tile.THIN_WALL_E,
    0x30: Tile.THIN_WALL_SE,
    0x0c: Tile.ICE,
    0x1a: Tile.ICE_CORNER_NW,
    0x1d: Tile.ICE_CORNER_SW,
    0x1c: Tile.ICE_CORNER_SE,
    0x1b: Tile.ICE_CORNER_NE,
    0x12: Tile.FORCE_FLOOR_N,
    0x14: Tile.FORCE_FLOOR_W,
    0x0d: Tile.FORCE_FLOOR_S,
    0x13: Tile.FORCE_FLOOR_E,
    0x32: Tile.FORCE_FLOOR_RANDOM,
    0x2d: Tile.GRAVEL,
    0x15: Tile.EXIT,
    0x68: Tile.BOOTS_WATER,
    0x69: Tile.BOOTS_FIRE,
    0x6a: Tile.BOOTS_ICE,
    0x6b: Tile.BOOTS_FORCE_FLOOR,
    0x16: Tile.LOCK_BLUE,
    0x17: Tile.LOCK_RED,
    0x18: Tile.LOCK_GREEN,
    0x19: Tile.LOCK_YELLOW,
    0x66: Tile.KEY_GREEN,
    0x67: Tile.KEY_YELLOW,
    0x21: Tile.THIEF,
    0x02: Tile.CHIP,
    0x2e: Tile.RECESSED_WALL,
    0x1e: Tile.WALL_BLUE_FAKE,
    0x22: Tile.SOCKET,
    0x0b: Tile.DIRT,
    0x2f: Tile.HINT,
    0x01: Tile.WALL,
    0x05: Tile.WALL_INVISIBLE,
    0x2c: Tile.WALL_HIDDEN,
    0x1f: Tile.WALL_BLUE_REAL,
    0x3a: Tile.FAKE_EXIT,
    0x31: Tile.CLONER,
    0x29: Tile.TELEPORTER,
    0x03: Tile.WATER,
    0x04: Tile.FIRE,
    0x2a: Tile.BOMB,
    0x0e: Tile.CLONER,
    0x0f: Tile.CLONER,
    0x10: Tile.CLONER,
    0x11: Tile.CLONER,
    0x39: Tile.FAKE_EXIT,
    0x3b: Tile.FAKE_EXIT,
}

TILE_REMAP_TOP = {
    0x0a: Actor(Entity.BLOCK, Direction.NORTH),
    0x0e: Actor(Entity.BLOCK, Direction.NORTH),
    0x0f: Actor(Entity.BLOCK, Direction.WEST),
    0x10: Actor(Entity.BLOCK, Direction.SOUTH),
    0x11: Actor(Entity.BLOCK, Direction.EAST),
    0x40: Actor(Entity.BUG, Direction.NORTH),
    0x41: Actor(Entity.BUG, Direction.WEST),
    0x42: Actor(Entity.BUG, Direction.SOUTH),
    0x43: Actor(Entity.BUG, Direction.EAST),
    0x44: Actor(Entity.FIREBALL, Direction.NORTH),
    0x45: Actor(Entity.FIREBALL, Direction.WEST),
    0x46: Actor(Entity.FIREBALL, Direction.SOUTH),
    0x47: Actor(Entity.FIREBALL, Direction.EAST),
    0x48: Actor(Entity.BALL, Direction.NORTH),
    0x49: Actor(Entity.BALL, Direction.WEST),
    0x4a: Actor(Entity.BALL, Direction.SOUTH),
    0x4b: Actor(Entity.BALL, Direction.EAST),
    0x4c: Actor(Entity.TANK, Direction.NORTH),
    0x4d: Actor(Entity.TANK, Direction.WEST),
    0x4e: Actor(Entity.TANK, Direction.SOUTH),
    0x4f: Actor(Entity.TANK, Direction.EAST),
    0x50: Actor(Entity.GLIDER, Direction.NORTH),
    0x51: Actor(Entity.GLIDER, Direction.WEST),
    0x52: Actor(Entity.GLIDER, Direction.SOUTH),
    0x53: Actor(Entity.GLIDER, Direction.EAST),
    0x54: Actor(Entity.TEETH, Direction.NORTH),
    0x55: Actor(Entity.TEETH, Direction.WEST),
    0x56: Actor(Entity.TEETH, Direction.SOUTH),
    0x57: Actor(Entity.TEETH, Direction.EAST),
    0x58: Actor(Entity.WALKER, Direction.NORTH),
    0x59: Actor(Entity.WALKER, Direction.WEST),
    0x5a: Actor(Entity.WALKER, Direction.SOUTH),
    0x5b: Actor(Entity.WALKER, Direction.EAST),
    0x5c: Actor(Entity.BLOB, Direction.NORTH),
    0x5d: Actor(Entity.BLOB, Direction.WEST),
    0x5e: Actor(Entity.BLOB, Direction.SOUTH),
    0x5f: Actor(Entity.BLOB, Direction.EAST),
    0x60: Actor(Entity.PARAMECIUM, Direction.NORTH),
    0x61: Actor(Entity.PARAMECIUM, Direction.WEST),
    0x62: Actor(Entity.PARAMECIUM, Direction.SOUTH),
    0x63: Actor(Entity.PARAMECIUM, Direction.EAST),
    0x6c: Actor(Entity.CHIP, Direction.NORTH),
    0x6d: Actor(Entity.CHIP, Direction.WEST),
    0x6e: Actor(Entity.CHIP, Direction.SOUTH),
    0x6f: Actor(Entity.CHIP, Direction.EAST),
}


class EncodeError(Exception):
    pass


@dataclass
class Link:
    btn_x: int
    btn_y: int
    linked_x: int
    linked_y: int


@dataclass
class Position:
    x: int
    y: int


Metadata = Union[str, List[Position], List[Link]]


@dataclass(frozen=True)
class MsLevelData:
    file: Path
    number: int
    time_limit: int
    required_chips: int
    top_layer: List[int]
    bottom_layer: List[int]

    title: str
    password: str
    hint: Optional[str]
    monsters: List[Position]
    trap_linkage: List[Link]
    cloner_linkage: List[Link]

    TIME_LIMIT_NONE = 0

    METADATA_TITLE = 3
    METADATA_TRAPS = 4
    METADATA_CLONERS = 5
    METADATA_PASSWORD = 6
    METADATA_HINT = 7
    METADATA_MONSTERS = 10


class DatFileReader:
    file: Path
    data: bytes
    pos: int

    def __init__(self, file: Path):
        self.file = file
        try:
            with open(file, "rb") as file:
                self.data = file.read()
        except IOError as e:
            raise EncodeError(f"couldn't read file: {e}")
        self.pos = 0

    def _read(self, n: int) -> int:
        val = 0
        for i in range(n):
            val |= self.data[self.pos] << (i * 8)
            self.pos += 1
        return val

    def _read_layer(self) -> List[int]:
        length = self._read(2)
        end = self.pos + length
        run_length = 0
        run_tile = -1
        data = []
        for i in range(Level.GRID_SIZE):
            if run_length > 0:
                data.append(run_tile)
                run_length -= 1
            else:
                tile = self._read(1)
                if tile == 0xff:
                    run_length = self._read(1) - 1
                    assert run_length >= 0
                    run_tile = self._read(1)
                    tile = run_tile
                data.append(tile)
        self.pos = end
        return data

    def _read_metadata_chunk(self) -> Tuple[int, Metadata]:
        chunk_id = self._read(1)
        length = self._read(1)
        data = self.data[self.pos:self.pos + length]
        self.pos += length
        if chunk_id == MsLevelData.METADATA_TITLE or chunk_id == MsLevelData.METADATA_HINT:
            return chunk_id, data[:-1].decode("ascii")
        elif chunk_id == MsLevelData.METADATA_PASSWORD:
            decrypted = bytes((b ^ 0x99 for b in data[:-1]))
            return chunk_id, decrypted.decode("ascii")
        elif chunk_id == MsLevelData.METADATA_TRAPS or chunk_id == MsLevelData.METADATA_CLONERS:
            links = []
            for i in range(0, length, 10 if chunk_id == MsLevelData.METADATA_TRAPS else 8):
                links.append(Link(data[i], data[i + 2], data[i + 4], data[i + 6]))
            return chunk_id, links
        elif chunk_id == MsLevelData.METADATA_MONSTERS:
            positions = []
            for i in range(0, length, 2):
                positions.append(Position(data[i], data[i + 1]))
            return chunk_id, positions

        # unknown or unsupported type, ignore
        raise EncodeError(f"unknown metadata chunk ID {chunk_id}")

    def _get_metadata(self, metadata: Dict[int, Metadata], chunk_id: int,
                      required_message: Optional[str] = None,
                      default: Optional[Metadata] = None) -> Optional[Metadata]:
        if chunk_id not in metadata:
            if required_message:
                raise EncodeError(required_message)
            else:
                return default
        return metadata[chunk_id]

    def _read_level(self) -> MsLevelData:
        number = self._read(2)
        time_limit = self._read(2)
        required_chips = self._read(2)
        self.pos += 2  # "unclear" field

        if time_limit == 0:
            time_limit = TileWorld.TIME_LIMIT_NONE
        elif time_limit >= 1000:
            raise EncodeError("level time limit over 999 seconds")
        else:
            time_limit *= TileWorld.TICKS_PER_SECOND

        top_layer = self._read_layer()
        bottom_layer = self._read_layer()

        metadata_length = self._read(2)
        metadata_end = self.pos + metadata_length
        all_metadata = {}
        while self.pos != metadata_end:
            chunk_id, metadata = self._read_metadata_chunk()
            all_metadata[chunk_id] = metadata

        title = self._get_metadata(all_metadata, MsLevelData.METADATA_TITLE,
                                   f"level {number} has no title")
        password = self._get_metadata(all_metadata, MsLevelData.METADATA_PASSWORD,
                                      f"level {number} has no password")
        hint = self._get_metadata(all_metadata, MsLevelData.METADATA_HINT)
        monsters = self._get_metadata(all_metadata, MsLevelData.METADATA_MONSTERS, default=[])
        trap_linkage = self._get_metadata(all_metadata, MsLevelData.METADATA_TRAPS, default=[])
        cloner_linkage = self._get_metadata(all_metadata, MsLevelData.METADATA_CLONERS, default=[])

        return MsLevelData(self.file, number, time_limit, required_chips, top_layer, bottom_layer,
                           title, password, hint, monsters, trap_linkage, cloner_linkage)

    def read_levels(self) -> List[MsLevelData]:
        magic = self._read(4)
        if magic != 0x0002aaac and magic != 0x0102aaac:
            raise EncodeError(f"invalid magic number, got {magic:#08x}")

        level_count = self._read(2)
        levels = []
        for i in range(level_count):
            length = self._read(2)
            end = self.pos + length
            levels.append(self._read_level())
            self.pos = end
        return levels


class DatFileWriter:
    data: bytearray
    levels_written: int
    warnings_count: int
    _last_level_start: int

    def __init__(self, name: str, level_count: int):
        self.data = bytearray()
        # signature
        self._write(0x5754, 2)

        # number of levels
        if not (1 <= level_count <= 256):
            raise EncodeError(f"there must be between 1 and 256 levels (got {level_count})")
        self._write(level_count, 1)

        # index
        self.levels_written = 0
        self._last_level_start = 0
        for i in range(level_count):
            self._write(0, 2)

        # title
        name = name.upper()
        if not (0 < len(name) < 12) or not re.fullmatch(r"[A-Za-z0-9.,!?'\"\-=#():; ]+", name):
            raise EncodeError("invalid level pack name")
        self.data += name.encode("ascii")
        self.data.append(0)  # nul terminator

        self.errors_count = 0
        self.warnings_count = 0

    def _write(self, val: int, n: int, at: int = -1) -> None:
        data = val.to_bytes(n, "little")
        if at != -1:
            self.data[at:at + n] = data
        else:
            self.data += data

    def _convert_tile(self, bottom: int, top: int, x: int, y: int) -> Tuple[Tile, Actor]:
        bottom_remap = TILE_REMAP_BOTTOM.get(bottom, None)
        top_remap = TILE_REMAP_TOP.get(top, None)

        if top_remap is None and bottom == 0x00:
            # could not find remapped ID in top layer but bottom layer is floor,
            # try to put the top tile on the bottom layer
            bottom_remap = TILE_REMAP_BOTTOM.get(top, None)
            top_remap = Actor.NONE
        elif bottom_remap is None and top == 0x00:
            # could not find remapped ID in bottom layer but top layer is empty,
            # try to put the bottom tile on the top layer
            bottom_remap = Tile.FLOOR
            top_remap = TILE_REMAP_TOP.get(bottom, None)

        # block cloner is defined as cloner + directional block
        # other tile in top or bottom layer is ignored
        if bottom in TILE_REMAP_BOTTOM and bottom in TILE_REMAP_TOP:
            return TILE_REMAP_BOTTOM[bottom], TILE_REMAP_TOP[bottom]
        elif top in TILE_REMAP_BOTTOM and top in TILE_REMAP_TOP:
            return TILE_REMAP_BOTTOM[top], TILE_REMAP_TOP[top]

        if top in [0x33, 0x34, 0x35] or bottom in [0x33, 0x34, 0x35]:
            # burned and drowned chip becomes wall since they are wall-acting
            bottom_remap = Tile.WALL
            top_remap = Actor.NONE
            print(f"  WARNING: found invalid wall-acting ID {top:#02x} at pos ({x}, {y})")
            self.warnings_count += 1
        elif bottom in [0x3c, 0x3d, 0x3e, 0x3f] or top in [0x3c, 0x3d, 0x3e, 0x3f]:
            # swimming chips is not allowed (replace with bomb which has nearly same effect,
            # with the difference that swimming chips is like a bomb that can't be removed by block)
            print(f"  ERROR: found swimming chip tile {top:#02x} at pos ({x}, {y})")
            bottom_remap = Tile.BOMB
            top_remap = Actor.NONE
            self.errors_count += 1

        if bottom_remap is None:
            print(f"  ERROR: invalid bottom tile {bottom} at pos ({x}, {y})")
            bottom_remap = Tile.FLOOR
            self.errors_count += 1
        if top_remap is None:
            top_remap = Actor.NONE
            print(f"  ERROR: invalid top tile {bottom} at pos ({x}, {y})")
            self.errors_count += 1

        return bottom_remap, top_remap

    def _convert_layers(self, level: MsLevelData) -> Tuple[List[Tile], List[Actor]]:
        top_layer = []
        bottom_layer = []
        i = 0
        for y in range(Level.GRID_HEIGHT):
            for x in range(Level.GRID_WIDTH):
                bottom, top = self._convert_tile(level.bottom_layer[i], level.top_layer[i], x, y)
                bottom_layer.append(bottom)
                top_layer.append(top)
                i += 1
        return bottom_layer, top_layer

    @staticmethod
    def _preprocess_layers_static_monsters(bottom_layer: List[Tile],
                                           top_layer: List[Actor]) -> None:
        """Use static variant if there's a non-directional monster
        (fireball, ball, blob) and this monster cannot move.
        Actors on buttons aren't replaced."""
        static_monsters = 0
        for x, y in TileWorld.iterate_grid():
            i = y * Level.GRID_WIDTH + x
            entity = top_layer[i].entity()
            tile = bottom_layer[i]
            if entity not in [Entity.FIREBALL, Entity.BALL, Entity.BLOB] or \
                    tile.is_button() or tile == Tile.STATIC_CLONER or tile == Tile.STATIC_TRAP:
                continue

            # list all neighboring tiles and actors in directions that the actor can move
            directions: Iterable[int]
            if entity == Entity.BALL:
                forward = top_layer[i].direction()
                directions = [forward, Direction.back(forward)]
            else:
                directions = range(4)

            tiles = []
            for direction in directions:
                nx, ny = TileWorld.get_pos_in_direction(x, y, direction)
                j = ny * Level.GRID_WIDTH + nx
                if 0 <= nx < Level.GRID_WIDTH and 0 <= ny < Level.GRID_HEIGHT:
                    tiles.append((bottom_layer[j], top_layer[j]))
                else:
                    tiles.append((Tile.WALL, Actor.NONE))

            if all((tile.is_chip_acting_wall() or
                    tile in [Tile.GRAVEL, Tile.STATIC_TRAP, Tile.EXIT, Tile.THIEF, Tile.HINT] or
                    tile == Tile.FIRE and entity != Entity.FIREBALL for tile, act in tiles)):
                # actor cannot moved, all directions blocked:
                # - chip acting walls: cannot exit and chip cannot free monster
                # - gravel, static trap, fire (if not a fireball), exit, thief, hint:
                #    these chip acting floor but monster acting wall tiles can't be replaced
                if entity == Entity.FIREBALL:
                    top_layer[i] = Actor.STATIC_FIREBALL
                elif entity == Entity.BALL:
                    top_layer[i] = Actor.STATIC_BALL
                elif entity == Entity.BLOB:
                    top_layer[i] = Actor.STATIC_BLOB
                static_monsters += 1

        if static_monsters:
            print(f"  added {static_monsters} static monsters")

    @staticmethod
    def _preprocess_layers_static_blocks(bottom_layer: List[Tile], top_layer: List[Actor]) -> None:
        """Replace unmoveable blocks with static blocks."""

        # Whether a block is cannot be moved depends on the neighboring tiles.
        # The patterns are checked for each direction, with the values being, in order:
        # forward, on the left, tile to the left of the tile forward.
        BLOCK = 0  # another block
        WALL = 1  # an acting wall, the map border, an exit, or a non-empty static trap
        ANY = 2  # any tile
        PATTERNS = [
            # diagonal walls case
            (WALL, WALL, ANY),
            # two walls cases
            (WALL, BLOCK, WALL),
            (BLOCK, WALL, WALL),
            # single wall cases
            (WALL, BLOCK, BLOCK),
            (BLOCK, BLOCK, WALL),
            (BLOCK, WALL, BLOCK),
            # three blocks case
            (BLOCK, BLOCK, BLOCK),
        ]

        # possible improvements: account for thin walls, ice walls

        static_blocks = 0
        invalidated = [True] * Level.GRID_SIZE
        while any(invalidated):
            for i in range(Level.GRID_SIZE):
                if not invalidated[i]:
                    continue
                invalidated[i] = False

                if top_layer[i].entity() != Entity.BLOCK:
                    continue

                tile = bottom_layer[i]
                if tile.is_slide() or tile.is_ice() or tile.is_button() or tile == Tile.CLONER:
                    # block on sliding floor may slide at start so we can't consider it static.
                    # blocks on cloner is probably a static cloner anyway.
                    continue

                x, y = i % Level.GRID_WIDTH, i // Level.GRID_WIDTH
                static = False
                for forward_dir in range(4):
                    right_dir = Direction.left(forward_dir)

                    pos_forward = TileWorld.get_pos_in_direction(x, y, forward_dir)
                    pos_right = TileWorld.get_pos_in_direction(x, y, right_dir)
                    pos_diag = TileWorld.get_pos_in_direction(*pos_forward, right_dir)
                    all_pos = [pos_forward, pos_right, pos_diag]

                    tiles = []
                    actors = []
                    for px, py in all_pos:
                        j = py * Level.GRID_WIDTH + px
                        if 0 <= px < Level.GRID_WIDTH and 0 <= py < Level.GRID_HEIGHT:
                            tiles.append(bottom_layer[j])
                            actors.append(top_layer[j])
                        else:
                            tiles.append(Tile.WALL)  # map border
                            actors.append(Actor.NONE)

                    for pattern in PATTERNS:
                        if all(p == ANY or p == BLOCK and actors[j].entity() == Entity.BLOCK or
                               p == WALL and (tiles[j].is_chip_acting_wall() or
                                              tiles[j] == Tile.EXIT or tiles[j] == Tile.STATIC_TRAP
                                              and actors[j].entity() != Entity.NONE)
                               for j, p in enumerate(pattern)):
                            top_layer[i] = Actor.STATIC_BLOCK
                            bottom_layer[i] = Tile.WALL  # there's no point leaving whatever it was
                            static_blocks += 1
                            static = True

                            # invalidate neighbor in each direction
                            # we need to do this since a static block becomes an acting wall
                            # and thus may create other static blocks
                            for neighbor_dir in range(4):
                                nx, ny = TileWorld.get_pos_in_direction(x, y, neighbor_dir)
                                if 0 <= nx < Level.GRID_WIDTH and 0 <= ny < Level.GRID_HEIGHT:
                                    invalidated[ny * Level.GRID_WIDTH + nx] = True
                            break

                    if static:
                        break

        if static_blocks:
            print(f"  added {static_blocks} static blocks")

    @staticmethod
    def _preprocess_layers_unlinked(level: MsLevelData, bottom_layer: List[Tile],
                                    top_layer: List[Actor]) -> None:
        """Replace unlinked cloners and traps with their static counterpart."""
        static_cloners = 0
        static_traps = 0
        links = {(link.linked_y * Level.GRID_WIDTH + link.linked_x)
                 for link in (level.trap_linkage + level.cloner_linkage)}
        for i in range(Level.GRID_SIZE):
            if bottom_layer[i] == Tile.CLONER:
                if i not in links:
                    bottom_layer[i] = Tile.STATIC_CLONER
                    static_cloners += 1
            elif bottom_layer[i] == Tile.TRAP and i not in links:
                bottom_layer[i] = Tile.WALL if top_layer[i].entity() == Entity.BLOCK \
                    else Tile.STATIC_TRAP
                static_traps += 1

        if static_cloners:
            print(f"  added {static_cloners} static cloners")
        if static_traps:
            print(f"  added {static_traps} static traps")

    @staticmethod
    def _preprocess_layers_ghost_blocks(bottom_layer: List[Tile],
                                        top_layer: List[Actor]) -> None:
        ghost_blocks = 0
        for i in range(Level.GRID_SIZE):
            tile = bottom_layer[i]
            actor = top_layer[i]
            if actor.entity() == Entity.BLOCK and not tile.is_button() and \
                    not tile.is_ice() and not tile.is_slide() and not tile == Tile.TRAP and \
                    not tile == Tile.CLONER:
                # block that won't immediately move and has no side effects: put a ghost block.
                top_layer[i] = actor.with_entity(Entity.BLOCK_GHOST)
                ghost_blocks += 1

        if ghost_blocks:
            print(f"  added {ghost_blocks} ghost blocks")

    def _preprocess_layers(self, level: MsLevelData,
                           bottom_layer: List[Tile], top_layer: List[Actor]) -> None:
        DatFileWriter._preprocess_layers_unlinked(level, bottom_layer, top_layer)
        DatFileWriter._preprocess_layers_static_blocks(bottom_layer, top_layer)
        DatFileWriter._preprocess_layers_static_monsters(bottom_layer, top_layer)
        # DatFileWriter._preprocess_layers_ghost_blocks(bottom_layer, top_layer)

        # count non-static actors
        actor_count = 0
        has_cloners = False
        for i in range(Level.GRID_SIZE):
            tile = bottom_layer[i]
            act = top_layer[i]
            if not tile.is_static() and \
                    (act.entity().is_on_actor_list() or act.entity() == Entity.CHIP):
                if act.entity() != Entity.NONE:
                    actor_count += 1
                if tile == Tile.CLONER:
                    has_cloners = True

        if actor_count > TileWorld.MAX_ACTORS:
            print(f"  ERROR: too many actors ({actor_count} > {TileWorld.MAX_ACTORS})")
            self.errors_count += 1
        elif has_cloners and actor_count > TileWorld.MAX_ACTORS - 20:
            print(f"  WARNING: maybe too many actors ({actor_count} and has cloners)")
            self.warnings_count += 1
        else:
            print(f"  {actor_count} active actors")

    def _write_layers(self, bottom_layer: List[Tile], top_layer: List[Actor]) -> int:
        # create top and bottom layer arrays
        top_data = bytearray()
        bottom_data = bytearray()

        block_size = numpy.lcm(8, Level.ENCODED_BITS_PER_TILE)
        bytes_per_block = block_size // 8
        tiles_per_block = block_size // Level.ENCODED_BITS_PER_TILE

        for i in range(0, Level.GRID_SIZE, tiles_per_block):
            top_block = 0
            bot_block = 0
            for j in range(tiles_per_block):
                top_block |= top_layer[i + j].value << (Level.ENCODED_BITS_PER_TILE * j)
                bot_block |= bottom_layer[i + j].value << (Level.ENCODED_BITS_PER_TILE * j)
            top_data += top_block.to_bytes(bytes_per_block, "little")
            bottom_data += bot_block.to_bytes(bytes_per_block, "little")

        # compress data
        data = bottom_data
        data += top_data
        compressed = lzss.encode(data)
        if lzss.decode(compressed) != data:
            raise EncodeError("compression error")
        self.data += compressed

        print(f"  layer compression: {len(data)}B -> {len(compressed)}B "
              f"({(len(compressed) - len(data)) / len(data):+.0%})")

        return len(compressed)

    def _write_linkage(self, linkage: List[Link]) -> None:
        if len(linkage) > 32:
            raise EncodeError(f"there must be less than 32 links (got {len(linkage)})")
        self._write(len(linkage), 1)
        for link in linkage:
            self._write(link.btn_x, 1)
            self._write(link.btn_y, 1)
            self._write(link.linked_x, 1)
            self._write(link.linked_y, 1)

    def write_level(self, level: MsLevelData) -> None:
        # write index entry
        start_pos = len(self.data)
        self._write(start_pos - self._last_level_start, 2, at=(self.levels_written * 2) + 3)
        self._last_level_start = start_pos

        self._write(level.time_limit, 2)
        self._write(level.required_chips, 2)
        self._write(0, 2)  # reserve space for layer data size

        if not re.fullmatch(r"[A-Z]{4}", level.password):
            raise EncodeError("password must be 4 letters")
        self.data += level.password.encode("ascii")

        self._write(0, 2 * 4)  # reserve space for index

        bottom_layer, top_layer = self._convert_layers(level)
        self._preprocess_layers(level, bottom_layer, top_layer)
        layer_data_size = self._write_layers(bottom_layer, top_layer)
        self._write(layer_data_size, 2, at=start_pos + 4)

        title = level.title.upper()
        if not (0 < len(title) < 40) or not re.fullmatch(r"[A-Za-z0-9.,!?'\"\-_=#():;* ]+", title):
            raise EncodeError("invalid characters in title or title too long")
        self._write(len(self.data) - start_pos, 2, at=start_pos + 10)
        self.data += level.title.encode("ascii")
        self.data.append(0)  # nul terminator

        if level.hint:
            if not (0 < len(level.hint) < 128) or \
                    not re.fullmatch(r"[A-Za-z0-9.,!?'\"\-_=#():;* \n]+", level.hint):
                raise EncodeError("invalid characters in hint or hint too long")
            self._write(len(self.data) - start_pos, 2, at=start_pos + 12)
            self.data += level.hint.encode("ascii")
            self.data.append(0)  # nul terminator

        self._write(len(self.data) - start_pos, 2, at=start_pos + 14)
        self._write_linkage(level.trap_linkage)

        self._write(len(self.data) - start_pos, 2, at=start_pos + 16)
        self._write_linkage(level.cloner_linkage)

        self.levels_written += 1

    def write_file(self, filename: Path) -> None:
        try:
            with open(filename, "wb") as file:
                file.write(self.data)
        except IOError as e:
            raise EncodeError(f"could not write file: {e}")


@dataclass(frozen=True)
class Config:
    input_file: Path
    output_file: Path
    name: str


def create_config(args: argparse.Namespace) -> Config:
    input_file = Path(args.input_file)
    if not input_file.exists() or not input_file.is_file():
        raise EncodeError(f"invalid input file '{args.input_file}'")

    output_file = Path(args.output_file)
    if not args.output_file:
        output_file = Path("output.dat")
    elif output_file.is_dir():
        output_file = Path(output_file, "output.dat")

    return Config(input_file, output_file, input_file.stem)


def readable_size(size: int) -> str:
    """Print size in human readable format, with units."""
    if size < 1024:
        return f"{size} B"
    size /= 1024
    for unit in ["k", "M", "G", "T"]:
        if abs(size) < 1024:
            return f"{size:.2f} {unit}B"
        size /= 1024
    raise ValueError()


def create_level_data(config: Config) -> bytes:
    levels = DatFileReader(config.input_file).read_levels()
    writer = DatFileWriter(config.name, len(levels))
    for i, level in enumerate(levels):
        print(f"LEVEL {config.name}/{level.number}")
        writer.write_level(level)
        print()
    writer.write_file(config.output_file)
    print(f"Conversion finished with {writer.errors_count} errors "
          f"and {writer.warnings_count} warnings")
    print(f"Level data written to {config.output_file} ({readable_size(len(writer.data))})")
    return writer.data


@dataclass(frozen=True)
class LevelPackObject(DataObject):
    file: Path
    name: str

    def pack(self) -> PackResult:
        try:
            with open(self.file, "rb") as file:
                sha = sha256(file.read()).hexdigest()
        except IOError as e:
            raise PackError(f"encoding error: could not read file: {e}")

        # level conversion takes some time, used cached result whenever possible.
        cache_dir = Path("assets/cache")
        cache_dir.mkdir(exist_ok=True)
        cached_file = cache_dir / f"{sha[:10]}.dat"
        if cached_file.exists():
            with open(cached_file, "rb") as file:
                return PackResult(file.read())

        config = Config(self.file, cached_file, self.name)
        try:
            data = create_level_data(config)
            return PackResult(data)
        except EncodeError as e:
            raise PackError(f"encoding error: {e}")

    def get_type_name(self) -> str:
        return "level pack"


def register_builder(packer) -> None:
    @packer.file_builder
    def level_pack(filename: Path, name: str):
        yield LevelPackObject(filename, name)


def main() -> None:
    args = parser.parse_args()
    config = create_config(args)
    create_level_data(config)


parser = argparse.ArgumentParser(description="Utility for converting Chip's Challenge DAT file "
                                             "to tworld format")
parser.add_argument(
    "input_file", action="store", type=str,
    help="Input DAT file.")
parser.add_argument(
    "-o", "--output", action="store", type=str, default="", dest="output_file",
    help=f"Output DAT file (output.dat by default)")

if __name__ == '__main__':
    try:
        main()
    except EncodeError as ex:
        print(f"ERROR: {ex}", file=sys.stderr)
        exit(1)
