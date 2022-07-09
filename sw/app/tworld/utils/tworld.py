# TILE WORLD IMPLEMENTATION
# The logic in this implementation is almost entirely based on the Lynx ruleset implementation
# of Tile Word 1.4.0. The aim was to confirm that the game could be implemented with a limited
# amount of RAM. As such, a lot of data structures are different from Tile World to save as much
# as possible on memory. Notably:
#
# - Actor list length limited to 128
# - Static actors are introduced to avoid this limit.
# - No animations, no sound effects.
# - cr->tdir and cr->fdir not used, only cr->dir
# - No per creature flags (slide token, reverse, pushed, teleported).
#   These are replaced by a state value and should be 100% equivalent.
# - Only actor position, stepping and state are stored in actor list,
#   the rest is taken from the grid (entity, direction).
# - Instead of "move laws" array, the tile IDs were remapped carefully to reduce
#   the amount of logic needed to determine whether the tile is a wall.
# - The rest should be nearly identical.
#
# Known bugs and affected levels:
# - Chip is swapped with wrong actor because there are static actors: CCLP4/71
# - Blob order is wrong because there are static blobs and non-static blobs: CCLP4/125
#

import time
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Tuple, Optional, Generator

import lzss


class TWException(Exception):
    pass


class Tile(Enum):
    """A tile goes on the bottom layer and is always static."""
    FLOOR = 0x00
    TRAP = 0x01
    TOGGLE_FLOOR = 0x02
    TOGGLE_WALL = 0x03
    # buttons
    BUTTON_GREEN = 0x04
    BUTTON_RED = 0x05
    BUTTON_BROWN = 0x06
    BUTTON_BLUE = 0x07
    # floor acting keys
    KEY_BLUE = 0x08
    KEY_RED = 0x09
    # thin wall
    THIN_WALL_N = 0x0c
    THIN_WALL_W = 0x0d
    THIN_WALL_S = 0x0e
    THIN_WALL_E = 0x0f
    THIN_WALL_SE = 0x10
    # ice
    ICE = 0x13
    ICE_CORNER_NW = 0x14
    ICE_CORNER_SW = 0x15
    ICE_CORNER_SE = 0x16
    ICE_CORNER_NE = 0x17
    # force floor
    FORCE_FLOOR_N = 0x18
    FORCE_FLOOR_W = 0x19
    FORCE_FLOOR_S = 0x1a
    FORCE_FLOOR_E = 0x1b
    FORCE_FLOOR_RANDOM = 0x1c
    # acting walls for monsters only
    GRAVEL = 0x1e
    EXIT = 0x1f
    BOOTS_WATER = 0x20
    BOOTS_FIRE = 0x21
    BOOTS_ICE = 0x22
    BOOTS_FORCE_FLOOR = 0x23
    # acting walls for monsters and blocks
    LOCK_BLUE = 0x24
    LOCK_RED = 0x25
    LOCK_GREEN = 0x26
    LOCK_YELLOW = 0x27
    KEY_GREEN = 0x2a
    KEY_YELLOW = 0x2b
    THIEF = 0x2c
    CHIP = 0x2d
    # acting walls for all actors
    RECESSED_WALL = 0x2e
    WALL_BLUE_FAKE = 0x2f
    SOCKET = 0x30
    DIRT = 0x31
    HINT = 0x32
    WALL = 0x33
    WALL_BLUE_REAL = 0x34
    WALL_HIDDEN = 0x35
    WALL_INVISIBLE = 0x36
    FAKE_EXIT = 0x37
    CLONER = 0x38
    # static
    STATIC_CLONER = 0x3a
    STATIC_TRAP = 0x3b
    # special
    TELEPORTER = 0x3c
    WATER = 0x3d
    FIRE = 0x3e
    BOMB = 0x3f
    # internal use only (not encodable)
    BLOCK = 0x40
    CHIP_DROWNED = 0x41
    CHIP_BURNED = 0x42
    CHIP_BOMBED = 0x43
    CHIP_SWIMMING_N = 0x44
    CHIP_SWIMMING_W = 0x45
    CHIP_SWIMMING_S = 0x46
    CHIP_SWIMMING_E = 0x47

    def variant(self) -> int:
        # variant for key, lock, boot, button, force floor, ice corner (0-3)
        return self.value & 0x3

    def is_key(self) -> bool:
        # also matches 0x0a, 0x0b, 0x28, 0x29, but these are unassigned for that purpose
        return (self.value & 0x1c) == 0x08

    def is_lock(self) -> bool:
        return (self.value & ~0x3) == Tile.LOCK_BLUE.value

    def is_boots(self) -> bool:
        return (self.value & ~0x3) == Tile.BOOTS_WATER.value

    def is_button(self) -> bool:
        return (self.value & ~0x3) == Tile.BUTTON_GREEN.value

    def is_thin_wall(self) -> bool:
        return Tile.THIN_WALL_N.value <= self.value <= Tile.THIN_WALL_SE.value

    def is_ice(self) -> bool:
        return Tile.ICE.value <= self.value <= Tile.ICE_CORNER_NE.value

    def is_ice_wall(self) -> bool:
        return (self.value & ~0x3) == Tile.ICE_CORNER_NW.value

    def is_slide(self) -> bool:
        return Tile.FORCE_FLOOR_N.value <= self.value <= Tile.FORCE_FLOOR_RANDOM.value

    def is_monster_acting_wall(self) -> bool:
        return 0x1e <= self.value <= 0x3a

    def is_block_acting_wall(self) -> bool:
        return 0x1f <= self.value <= 0x3a

    def is_chip_acting_wall(self) -> bool:
        return 0x33 <= self.value <= 0x3a

    def is_revealable_wall(self) -> bool:
        # wall hidden & wall blue real
        return (self.value & ~0x1) == 0x34

    def is_static(self) -> bool:
        return (self.value & ~0x1) == 0x3a

    def is_toggle_tile(self) -> bool:
        return (self.value & ~0x1) == 0x02

    def with_toggle_state(self, state: int) -> "Tile":
        assert self.is_toggle_tile()
        return Tile(self.value ^ state)

    def toggle_state(self) -> "Tile":
        return self.with_toggle_state(0x1)

    def __repr__(self):
        return self.name


class Entity(Enum):
    """An entity is the type of an actor."""
    NONE = 0x00
    CHIP = 0x04
    STATIC = 0x0c
    BLOCK_GHOST = 0x10
    BLOCK = 0x14
    BUG = 0x18
    PARAMECIUM = 0x1c
    GLIDER = 0x20
    FIREBALL = 0x24
    BALL = 0x28
    BLOB = 0x2c
    TANK = 0x30
    TANK_REVERSED = 0x34
    WALKER = 0x38
    TEETH = 0x3c

    def is_tank(self) -> bool:
        """Tank or reversed tank."""
        return (self.value & 0x38) == 0x30

    def is_block(self) -> bool:
        """Block or ghost block."""
        return (self.value & 0x38) == 0x10

    def reverse_tank(self) -> "Entity":
        assert self.is_tank()
        return Entity(self.value ^ 0x04)

    def is_monster_or_block(self) -> bool:
        return self.value >= Entity.BLOCK_GHOST.value

    def is_monster(self) -> bool:
        return self.value > Entity.BLOCK.value

    def is_on_actor_list(self) -> bool:
        """Except chip, entities that are initially added to the actor list."""
        return self.value >= Entity.BLOCK.value


@dataclass(frozen=True)
class Actor:
    """An actor is a top layer tile, it contains entity and direction information."""
    value: int

    def __init__(self, *args):
        # value: int
        # entity: Entity, direction: int
        if len(args) == 1:
            assert isinstance(args[0], int)
            value = args[0]
        elif len(args) == 2:
            direction = args[1]
            assert 0 <= direction <= 3
            if isinstance(args[0], int):
                value = args[0] | direction
            elif isinstance(args[0], Entity):
                value = args[0].value | direction
            else:
                raise ValueError
        else:
            raise ValueError
        object.__setattr__(self, "value", value)
        self.entity()  # build entity object to validate

    def entity(self) -> Entity:
        return Entity(self.value & ~0x3)

    def direction(self) -> int:
        return self.value & 0x3

    def in_direction(self, direction: int) -> "Actor":
        return Actor(self.value & ~0x3, direction)

    def is_block(self) -> bool:
        # static block, ghost block, regular block
        return 0x0f <= self.value <= 0x17

    def with_entity(self, entity: Entity) -> "Actor":
        return Actor(entity.value, self.value & 0x3)

    def __repr__(self) -> str:
        return f"{self.entity().name.lower()} " \
               f"({['north', 'west', 'south', 'east'][self.direction()]})"


# for empty top layer tile
Actor.NONE = Actor(0)
# for a top layer tile where an animation is occuring
Actor.ANIMATION = Actor(1)

Actor.STATIC_FIREBALL = Actor(Entity.STATIC, 0)
Actor.STATIC_BALL = Actor(Entity.STATIC, 1)
Actor.STATIC_BLOB = Actor(Entity.STATIC, 2)
Actor.STATIC_BLOCK = Actor(Entity.STATIC, 3)


@dataclass(frozen=True)
class Link:
    """A link between a button and a trap/cloner."""
    btn_x: int
    btn_y: int
    link_x: int
    link_y: int


@dataclass(frozen=True)
class Level:
    """Level data read from a DAT level pack file."""
    pack_file: Path
    number: int
    time_limit: int
    required_chips: int
    title: str
    password: str
    hint: str
    trap_linkage: List[Link]
    cloner_linkage: List[Link]
    bottom_layer: List[Tile]  # 768b
    top_layer: List[Actor]  # 768b

    GRID_WIDTH = 32
    GRID_HEIGHT = 32
    GRID_SIZE = GRID_WIDTH * GRID_HEIGHT

    # Number of bits per tile in encoded layer data
    # Note that 8 bits per tile improves compression significantly (around -10% size on pack),
    # but also complicates decoding since data has to be compressed to 6 bits per tile at runtime.
    ENCODED_BITS_PER_TILE = 6
    ENCODED_TILE_MASK = (1 << ENCODED_BITS_PER_TILE) - 1


class EndCause(Enum):
    """Ways for chip to die."""
    NONE = 0
    COMPLETE = 1
    DROWNED = 2
    BURNED = 3
    BOMBED = 4
    OUTOFTIME = 5
    COLLIDED = 6


@dataclass
class ActiveActor:
    """An entry in the actor list. Entity and direction information can be obtained by
    checking the top tile at the actor's position."""
    x: int  # x position, 5 bits [0:4]
    y: int  # y position, 5 bits [7:11]
    step: int = field(default=0)  # 4 bits [12:15]
    state: int = field(default=0)  # state [5:6]

    # default state
    STATE_NONE = 0x0
    # hidden state, when the actor is dead
    # hidden actor entries are skipped and are reused when spawning a new actor.
    STATE_HIDDEN = 0x1
    # moved state, when the actor has chosen a move during stepping (vs. not moving).
    # this also applies when a move is forced on an actor.
    STATE_MOVED = 0x2
    # teleported state, when the actor has just been teleported.
    STATE_TELEPORTED = 0x3


@dataclass
class MovingActor:
    """Container used during step processing to store information about an actor for
    easy access. There is always one or two instances of this container so size is not an issue."""
    index: int
    x: int
    y: int
    step: int
    entity: Entity
    direction: int
    state: int

    # temporary extra state used to indicate that the actor has died and it's tile
    # should be replaced by an animation tile. (note: STATE_DIED & 0x3 == STATE_HIDDEN)
    STATE_DIED = 0x5

    # temporary extra state used to indicate a ghost block to be removed from the actor list.
    # unlike STATE_DIED, the actor is not replaced by an animation.
    # (note: STATE_GHOST & 0x3 == STATE_HIDDEN)
    STATE_GHOST = 0x9

    def actor(self) -> Actor:
        return Actor(self.entity, self.direction)


class Direction:
    """A direction is an integer from 0 to 3.
    Direction masks are also used for taking the input state and defining directional walls."""
    NORTH = 0
    WEST = 1
    SOUTH = 2
    EAST = 3

    NORTH_MASK = 1 << NORTH
    WEST_MASK = 1 << WEST
    SOUTH_MASK = 1 << SOUTH
    EAST_MASK = 1 << EAST

    VERTICAL_MASK = NORTH_MASK | SOUTH_MASK
    HORIZONTAL_MASK = WEST_MASK | EAST_MASK

    @staticmethod
    def to_mask(direction: int) -> int:
        # can be implemented as a lookup table for performance
        assert Direction.NORTH <= direction <= Direction.EAST
        return 1 << direction

    @staticmethod
    def from_mask(direction: int) -> int:
        # log2, implemented as lookup table for performance
        d = [-1, Direction.NORTH, Direction.WEST, -1,
             Direction.SOUTH, -1, -1, -1, Direction.EAST][direction]
        assert d != -1
        return d

    @staticmethod
    def back(direction: int) -> int:
        return [Direction.SOUTH, Direction.EAST, Direction.NORTH, Direction.WEST][direction]

    @staticmethod
    def right(direction: int) -> int:
        return (direction - 1) % 4

    @staticmethod
    def left(direction: int) -> int:
        return (direction + 1) % 4


Position = Tuple[int, int]


class TileWorld:
    """Tile World game logic implementation."""
    # level data is always stored in flash, only the address is known.
    # it is accessed rarely enough that flash read performance doesn't matter.
    # (accessed for title, hint, password, links, and when loading the level)
    level: Level  # 3B

    # grid data
    bottom_layer: List[Tile]  # 32x32x6 = 768B
    top_layer: List[Actor]  # 32x32x6 = 768B
    # actor list
    actors: List[ActiveActor]  # 128x16 = 256B
    # current time, in ticks
    current_time: int  # 3B
    # number of required chips left
    chips_left: int  # 2B
    # number of keys held (in order: blue, red, green, yellow)
    keys: List[int]  # 4B
    # boots held (a bitfield)
    boots: int  # 1B

    # game flags (FLAG_* constants below)
    flags: int  # 1B
    # next direction for random force floor
    random_slide_dir: int  # 1B
    # position after chip moved (cached)
    chip_new_pos_x: int  # 1B
    chip_new_pos_y: int  # 1B
    # direction of forced move for chip
    chip_last_direction: int
    # index of actor that collided with chip, or INDEX_NONE if none.
    collided_with: int  # 1B
    # actor that collision occured with.
    collided_actor: Actor  # 1B
    # ticks since chip last move (used to go back to rest position)
    ticks_since_move: int  # 1B
    # cause of death (or NONE if not dead)
    end_cause: EndCause  # 1B
    # currently active directions (bitfield)
    input_state: int  # 1B
    # last chip direction (since last start_movement call)
    last_chip_direction: int
    # saved chip tile in special teleporter case to prevent chip from disappearing.
    teleported_chip: Actor
    # index of actor currently springing a trap, or INDEX_NONE if none.
    actor_springing_trap: int

    # other, not in memory
    silent: bool
    # stepping value 0-7 (affects teeth only)
    stepping: int
    # initial direction for random slide
    initial_random_slide_dir: int
    # random number generator state
    prng_value0: int
    prng_value1: int
    prng_value2: int

    # maximum actors
    MAX_ACTORS = 128
    # if set, cloners aren't allowed to spawn more actors if limit is reached.
    MAX_ACTORS_STRICT_CLONERS = True
    # if set, actors aren't created initially if limit is reached.
    MAX_ACTORS_STRICT_INIT = True

    FLAG_TOGGLE_STATE = 1 << 0  # indicates toggle floor/wall (=0x1, this is important)
    FLAG_TURN_TANKS = 1 << 1  # indicates that there may be "reverse tanks" on the grid
    FLAG_CHIP_SELF_MOVED = 1 << 2  # indicates that chip has moved by himself
    FLAG_CHIP_FORCE_MOVED = 1 << 3  # indicates that chip move has been forced
    FLAG_CHIP_CAN_UNSLIDE = 1 << 4  # indicates that chip can override force floor direction
    FLAG_CHIP_STUCK = 1 << 5  # indicates that chip is stuck on a teleporter
    FLAG_NO_TIME_LIMIT = 1 << 7  # indicates that the level is untimed

    # blocked direction for thin walls and ice corners
    THIN_WALL_DIR_FROM = [
        Direction.NORTH_MASK,  # thin wall north
        Direction.WEST_MASK,  # thin wall west
        Direction.SOUTH_MASK,  # thin wall south
        Direction.EAST_MASK,  # thin wall east
        Direction.SOUTH_MASK | Direction.EAST_MASK,  # thin wall south east
        Direction.NORTH_MASK | Direction.WEST_MASK,  # ice wall north west
        Direction.SOUTH_MASK | Direction.WEST_MASK,  # ice wall south west
        Direction.SOUTH_MASK | Direction.EAST_MASK,  # ice wall south east
        Direction.NORTH_MASK | Direction.EAST_MASK,  # ice wall north east
    ]
    THIN_WALL_DIR_TO = [
        Direction.SOUTH_MASK,  # thin wall north
        Direction.EAST_MASK,  # thin wall west
        Direction.NORTH_MASK,  # thin wall south
        Direction.WEST_MASK,  # thin wall east
        Direction.NORTH_MASK | Direction.WEST_MASK,  # thin wall south east
        Direction.SOUTH_MASK | Direction.EAST_MASK,  # ice wall north west
        Direction.NORTH_MASK | Direction.EAST_MASK,  # ice wall south west
        Direction.NORTH_MASK | Direction.WEST_MASK,  # ice wall south east
        Direction.SOUTH_MASK | Direction.WEST_MASK,  # ice wall north east
    ]
    # turn direction as function of ice wall type and incoming direction.
    ICE_WALL_TURN = [
        # north, west, south, east
        Direction.EAST, Direction.SOUTH, None, None,  # ice wall north west
        None, Direction.NORTH, Direction.EAST, None,  # ice wall south west
        None, None, Direction.WEST, Direction.NORTH,  # ice wall south east
        Direction.WEST, None, None, Direction.SOUTH,  # ice wall north east
    ]

    # return value for _start_movement and _perform_move
    RESULT_FAIL = 0  # failed to move, still alive
    RESULT_SUCCESS = 1  # move successfully or remained stationary successfully
    RESULT_DIED = 2  # died as result of move

    # flags used by _can_move
    CMM_START_MOVEMENT = 1 << 0  # was called as part of _start_movement
    CMM_PUSH_BLOCKS = 1 << 1  # blocks should be pushed (i.e. change the direction but not position)
    CMM_PUSH_BLOCKS_NOW = 1 << 2  # blocks should be moved
    CMM_RELEASING = 1 << 3  # was called as result of trap or cloner release
    CMM_CLEAR_ANIM = 1 << 4  # animation on target tile should be cleared
    CMM_PUSH_BLOCKS_ALL = CMM_PUSH_BLOCKS | CMM_PUSH_BLOCKS_NOW  # push & move blocks

    INDEX_NONE = 0xff
    CHIP_NEW_POS_NONE = 0xff

    # how many ticks per game time "second".
    TICKS_PER_SECOND = 20

    # how many ticks after chip last move before going to rest position.
    CHIP_REST_TICKS = 15
    # direction assumed by resting chip.
    CHIP_REST_DIRECTION = Direction.SOUTH

    TIME_LIMIT_NONE = 0xffff

    def __init__(self, level: Level, silent: bool = False):
        self.level = level
        self.silent = silent
        self.stepping = 0
        self.initial_random_slide_dir = Direction.NORTH
        self.prng_value0 = time.time_ns() & 0x7fffffff
        self.prng_value1 = 0
        self.prng_value2 = 0
        self.restart()

    @staticmethod
    def _warn(message: str) -> None:
        print(f"WARNING: {message}")

    @staticmethod
    def iterate_grid() -> Generator[int, None, None]:
        for y in range(Level.GRID_HEIGHT):
            for x in range(Level.GRID_WIDTH):
                yield x, y

    def get_bottom_tile(self, x: int, y: int):
        return self.bottom_layer[y * Level.GRID_WIDTH + x]

    def get_top_tile(self, x: int, y: int):
        return self.top_layer[y * Level.GRID_WIDTH + x]

    def _set_bottom_tile(self, x: int, y: int, value: Tile) -> None:
        self.bottom_layer[y * Level.GRID_WIDTH + x] = value

    def _set_top_tile(self, x: int, y: int, value: Actor) -> None:
        self.top_layer[y * Level.GRID_WIDTH + x] = value

    def has_water_boots(self) -> bool:
        return (self.boots & (1 << 0)) != 0

    def has_fire_boots(self) -> bool:
        return (self.boots & (1 << 1)) != 0

    def has_ice_boots(self) -> bool:
        return (self.boots & (1 << 2)) != 0

    def has_slide_boots(self) -> bool:
        return (self.boots & (1 << 3)) != 0

    def is_game_over(self) -> bool:
        return self.end_cause != EndCause.NONE

    def get_chip(self) -> ActiveActor:
        return self.actors[0]

    def current_pos(self) -> Position:
        chip = self.get_chip()
        return chip.x, chip.y

    def restart(self) -> None:
        # initialize grid
        self.bottom_layer = self.level.bottom_layer[:]
        self.top_layer = self.level.top_layer[:]

        self.flags = 0

        # initialize general state
        if self.level.time_limit == TileWorld.TIME_LIMIT_NONE:
            self.flags |= TileWorld.FLAG_NO_TIME_LIMIT
        self.current_time = 0
        self.chips_left = self.level.required_chips
        self.keys = [0] * 4
        self.boots = 0

        self.random_slide_dir = self.initial_random_slide_dir

        self.collided_with = TileWorld.INDEX_NONE
        self.collided_actor = Actor.NONE
        self.chip_last_direction = Direction.SOUTH
        self.ticks_since_move = 0
        self.end_cause = EndCause.NONE
        self.last_chip_direction = 0
        self.teleported_chip = Actor.NONE
        self.actor_springing_trap = TileWorld.INDEX_NONE

        self.prng_value1 = 0
        self.prng_value2 = 0

        self._build_actor_list()

    def _build_actor_list(self) -> None:
        """Build actor list in reading order (monster list is not used).
        Exclude all actors on static tiles, but don't exchange chip with the first actor if the
        first actor was made static by the conversion process."""
        self.actors = []
        chip_index = -1
        stop = False
        for y in range(Level.GRID_WIDTH):
            for x in range(Level.GRID_HEIGHT):
                entity = self.get_top_tile(x, y).entity()
                if entity == Entity.CHIP:
                    chip_index = len(self.actors)
                elif not entity.is_on_actor_list():
                    continue
                elif self.get_bottom_tile(x, y).is_static():
                    continue
                self.actors.append(ActiveActor(x, y))

                if TileWorld.MAX_ACTORS_STRICT_INIT and len(self.actors) == TileWorld.MAX_ACTORS:
                    self._warn("max actors reached on level start")
                    stop = True
                    break
            if stop:
                break

        # swap chip with first actor on list
        if chip_index == -1:
            raise TWException("chip not found in level data")
        if chip_index > 0:
            temp = self.actors[0]
            self.actors[0] = self.actors[chip_index]
            self.actors[chip_index] = temp

        if not TileWorld.MAX_ACTORS_STRICT_INIT and len(self.actors) > TileWorld.MAX_ACTORS:
            TileWorld._warn(f"more than {TileWorld.MAX_ACTORS} actors (got {len(self.actors)})")

    def step(self, input_state: int) -> None:
        """Advance game state by a single tick."""
        self.input_state = input_state

        # check for out of time condition
        if self.current_time == self.level.time_limit * TileWorld.TICKS_PER_SECOND and \
                not (self.flags & TileWorld.FLAG_NO_TIME_LIMIT):
            self.end_cause = EndCause.OUTOFTIME
            return

        self._step_check()
        self._prestep()
        self._choose_all_moves()
        self._perform_all_moves()
        self._teleport_all()

        # update time, even if there's no time limit, as time is used as a reference in some places.
        # time will underflow in that case and it's fine.
        self.current_time += 1

    def _step_check(self) -> None:
        """Sanity check at the start of a step. Optional, no side effects on state."""
        all_pos = [(act.x, act.y) for act in self.actors if act.state != ActiveActor.STATE_HIDDEN]
        if len(all_pos) > len(set(all_pos)) + (1 if self.teleported_chip != Actor.NONE else 0):
            raise TWException(f"actors at the same position at time {self.current_time}")

        for i, act in enumerate(self.actors):
            if self.get_top_tile(act.x, act.y).entity() == Entity.NONE and \
                    act.state != ActiveActor.STATE_HIDDEN:
                if i == 0 and self.teleported_chip != Actor.NONE:
                    continue
                raise TWException(f"actor at ({act.x}, {act.y}) has no entity")

        if self.teleported_chip == Actor.NONE:
            chip = self.get_chip()
            if self.get_top_tile(chip.x, chip.y).entity() != Entity.CHIP:
                raise TWException(f"chip is not first in actor list")

    def _prestep(self) -> None:
        """Finish applying changes from last step before making a new step."""
        # update toggle wall/floor according to toggle state
        if self.flags & TileWorld.FLAG_TOGGLE_STATE:
            self.flags &= ~TileWorld.FLAG_TOGGLE_STATE
            for x, y in TileWorld.iterate_grid():
                tile = self.get_bottom_tile(x, y)
                if tile.is_toggle_tile():
                    self._set_bottom_tile(x, y, tile.toggle_state())

        # if needed, transform "reverse tanks" to normal tanks in the opposite direction.
        if self.flags & TileWorld.FLAG_TURN_TANKS:
            self.flags &= ~TileWorld.FLAG_TURN_TANKS
            for i in range(len(self.actors)):
                act = self.actors[i]
                if act.state == ActiveActor.STATE_HIDDEN:
                    continue
                mact = self._create_moving_actor(i)
                if mact.entity == Entity.TANK_REVERSED:
                    mact.entity = Entity.TANK
                    if mact.step <= 0:
                        # don't turn tanks in between moves
                        mact.direction = Direction.back(mact.direction)
                    self._destroy_moving_actor(mact)

        self.chip_new_pos_x = TileWorld.CHIP_NEW_POS_NONE
        self.chip_new_pos_y = TileWorld.CHIP_NEW_POS_NONE

    def _choose_all_moves(self) -> None:
        """Choose a move for all actors."""
        for i in reversed(range(len(self.actors))):
            act = self.actors[i]
            if act.state == ActiveActor.STATE_HIDDEN:
                if act.step > 0:
                    # "animated" state delay
                    act.step -= 1
                continue
            prev_state = act.state
            act.state = ActiveActor.STATE_NONE
            if act.step <= 0:
                mact = self._create_moving_actor(i)
                self._choose_move(mact, prev_state == ActiveActor.STATE_TELEPORTED)
                self._destroy_moving_actor(mact)

    def _perform_all_moves(self) -> None:
        """Attempt to perform chosen move for all actors."""
        for i in reversed(range(len(self.actors))):
            act = self.actors[i]
            if act.state == ActiveActor.STATE_HIDDEN:
                continue
            mact = self._create_moving_actor(i)
            result = self._perform_move(mact, 0)
            persist = True
            if result != TileWorld.RESULT_DIED and mact.step <= 0 and \
                    self.get_bottom_tile(mact.x, mact.y) == Tile.BUTTON_BROWN:
                # if a block is on a trap button and chip pushes it off while springing the trap,
                # the block will be pushed and persisted then. Do not persist it in that case,
                # since the instance of MovingActor we have here has an outdated position.
                # Make sure of this by saving the index of the current block and checking it
                # in _can_push_block.
                self.actor_springing_trap = mact.index
                self._spring_trap(mact.x, mact.y)
                persist = self.actor_springing_trap != TileWorld.INDEX_NONE
                self.actor_springing_trap = TileWorld.INDEX_NONE
            if persist:
                self._destroy_moving_actor(mact)

    def _teleport_all(self) -> None:
        """Teleport all actors on a teleporter."""
        if self.is_game_over():
            # if collision occured with chip on teleporter tile,
            # don't teleport monster that caused it.
            return

        for i in reversed(range(len(self.actors))):
            act = self.actors[i]
            if act.state == ActiveActor.STATE_HIDDEN or act.step > 0:
                continue
            if self.get_bottom_tile(act.x, act.y) == Tile.TELEPORTER:
                mact = self._create_moving_actor(i)
                self._teleport_actor(mact)
                self._destroy_moving_actor(mact)

    def _spawn_actor(self) -> Optional[MovingActor]:
        """Create a new actor if possible. The actor must be destroyed properly afterwards
        if it's modified, otherwise it's not necessary since it spawns as hidden."""
        # reuse a hidden (dead) actor in the list if possible
        for i, act in enumerate(self.actors):
            if act.state == ActiveActor.STATE_HIDDEN and act.step == 0:
                return self._create_moving_actor(i)

        if TileWorld.MAX_ACTORS_STRICT_CLONERS and len(self.actors) >= TileWorld.MAX_ACTORS:
            # levels should be made in a way that this never happens!
            TileWorld._warn(f"maximum number of actors reached ({TileWorld.MAX_ACTORS})")
            return None

        # add a new actor at the end of list
        self.actors.append(ActiveActor(0, 0, 0, ActiveActor.STATE_HIDDEN))
        return MovingActor(len(self.actors) - 1, 0, 0, 0, Entity.NONE, 0, ActiveActor.STATE_HIDDEN)

    def _create_moving_actor(self, index: int) -> MovingActor:
        """Create a 'moving actor' container for the actor at an index in the actor list.
        Any changes to this container must be persisted through `_destroy_moving_actor`."""
        act = self.actors[index]
        act_tile = self.get_top_tile(act.x, act.y)
        entity = act_tile.entity()
        direction = act_tile.direction()
        return MovingActor(index, act.x, act.y, act.step, entity, direction, act.state)

    def _destroy_moving_actor(self, mact: MovingActor) -> None:
        """Persist any changes to a moving actor container to the actor list."""
        act = self.actors[mact.index]
        act.x, act.y, act.step, = mact.x, mact.y, mact.step
        act.state = mact.state & 0x3  # STATE_DIED/STATE_GHOST becomes STATE_HIDDEN
        act_tile = Actor.NONE
        if mact.state == MovingActor.STATE_DIED:
            act_tile = Actor.ANIMATION
        elif mact.state != ActiveActor.STATE_HIDDEN:
            act_tile = mact.actor()
        self._set_top_tile(mact.x, mact.y, act_tile)

    @staticmethod
    def get_pos_in_direction(x: int, y: int, direction: int) -> Position:
        """Returns the position of a neighbor tile at position in the given direction."""
        return x + [0, -1, 0, 1][direction], y + [-1, 0, 1, 0][direction]

    @staticmethod
    def _get_new_actor_pos(act: MovingActor, direction: int = None) -> Position:
        """Returns the new position for an actor moving in a direction (its own by default)."""
        if direction is None:
            direction = act.direction
        return TileWorld.get_pos_in_direction(act.x, act.y, direction)

    def _lookup_actor(self, x: int, y: int,
                      include_animated: bool = False) -> Optional[MovingActor]:
        """Creates and return a moving actor container for the actor at a position.
        Any changes must be persisted through _destroy_moving_actor()."""
        for i, act in enumerate(self.actors):
            if act.x == x and act.y == y:
                if act.state != ActiveActor.STATE_HIDDEN or \
                        (include_animated and act.step != 0):
                    return self._create_moving_actor(i)
        return None

    def _stop_death_animation(self, x: int, y: int) -> None:
        """Clear the animation on the top tile at a position, if any."""
        for act in self.actors:
            if act.x == x and act.y == y:
                act.step = 0
        self._set_top_tile(x, y, Actor.NONE)

    def _can_move(self, act: MovingActor, direction: int, flags: int) -> bool:
        """Returns true if the actor is allowed to move in the given direction.
        Flags can be set to set the context from which the move is performed."""
        px, py = TileWorld._get_new_actor_pos(act, direction)

        if px < 0 or px >= Level.GRID_WIDTH or py < 0 or py >= Level.GRID_HEIGHT:
            # cannot exit map borders
            return False

        tile_from = self.get_bottom_tile(act.x, act.y)
        tile_to = self.get_bottom_tile(px, py)

        if (tile_from == Tile.TRAP or tile_from == Tile.CLONER) and \
                not (flags & TileWorld.CMM_RELEASING):
            return False

        if tile_from == Tile.STATIC_TRAP:
            return False

        if tile_to.is_toggle_tile() and tile_to.with_toggle_state(
                self.flags & TileWorld.FLAG_TOGGLE_STATE) == Tile.TOGGLE_WALL:
            # since toggle state can change multiple time per step, only a flag is changed
            # instead of the whole grid for consistent execution time.
            return False

        if tile_from.is_slide() and (act.entity != Entity.CHIP or not self.has_slide_boots()) \
                and self._get_slide_dir(tile_from, False) == Direction.back(direction):
            # can't move back on slide floor when overriding forced movement
            return False

        # thin wall / ice corner directional handling
        thin_wall = 0
        if tile_from.is_thin_wall():
            thin_wall |= TileWorld.THIN_WALL_DIR_FROM[tile_from.value - Tile.THIN_WALL_N.value]
        elif tile_from.is_ice_wall():
            thin_wall |= TileWorld.THIN_WALL_DIR_FROM[
                tile_from.value - Tile.ICE_CORNER_NW.value + 5]
        if tile_to.is_thin_wall():
            thin_wall |= TileWorld.THIN_WALL_DIR_TO[tile_to.value - Tile.THIN_WALL_N.value]
        elif tile_to.is_ice_wall():
            thin_wall |= TileWorld.THIN_WALL_DIR_TO[tile_to.value - Tile.ICE_CORNER_NW.value + 5]
        if thin_wall & Direction.to_mask(direction):
            return False

        if act.entity == Entity.CHIP:
            if tile_to.is_chip_acting_wall() and not tile_to.is_revealable_wall():
                return False
            if tile_to == Tile.SOCKET and self.chips_left > 0:
                return False
            if tile_to.is_lock() and self.keys[tile_to.variant()] == 0:
                return False

            # check if there's another actor on destination tile
            other = self._lookup_actor(px, py, include_animated=True)
            if not other and self.get_top_tile(px, py).entity() == Entity.BLOCK_GHOST:
                # no actor there but there is a ghost block. Add it to the actor list if possible.
                # Levels should be made so that a ghost block can always be created, otherwise
                # it won't be spawned and won't be moved!
                new_block = self._spawn_actor()
                if new_block:
                    new_block.entity = Entity.BLOCK_GHOST
                    new_block.x, new_block.y = px, py
                    new_block.state = ActiveActor.STATE_NONE
                    other = new_block

            if other:
                if other.state == ActiveActor.STATE_HIDDEN:
                    if other.step > 0:
                        # "animated" actors block chip
                        return False
                elif other.entity.is_block():
                    if not self._can_push_block(other, direction, flags & ~TileWorld.CMM_RELEASING):
                        if other.entity == Entity.BLOCK_GHOST:
                            # ghost block just created can't be moved: hide it immediately
                            self.actors[other.index].state = ActiveActor.STATE_HIDDEN
                        return False
            # static blocks are always put on a wall so they are no concern here

            if tile_to.is_revealable_wall():
                if flags & TileWorld.CMM_START_MOVEMENT:
                    # reveal hidden wall or blue wall
                    self._set_bottom_tile(px, py, Tile.WALL)
                return False

            if self.flags & TileWorld.FLAG_CHIP_STUCK:
                # chip is stuck on teleporter forever
                return False

            return True

        elif act.entity.is_block():
            if act.step > 0:
                # block cannot move while in-between moves (when called from _can_push_block).
                return False
            if tile_to.is_block_acting_wall():
                return False

        else:
            if tile_to.is_monster_acting_wall():
                return False
            if tile_to == Tile.FIRE and act.entity != Entity.FIREBALL:
                # fire is treated as a wall by all except fireball
                return False

        other = self.get_top_tile(px, py)
        if other.entity().is_monster_or_block():
            # there's already a monster or a block there (location claimed)
            return False
        if (flags & TileWorld.CMM_CLEAR_ANIM) and other == Actor.ANIMATION:
            self._stop_death_animation(px, py)

        return True

    def _can_push_block(self, block: MovingActor, direction: int, flags: int) -> bool:
        """Returns true if the block is allowed to be pushed in the given direction.
        If flags include CMM_PUSH_BLOCKS, block direction is changed.
        If flags include CMM_PUSH_BLOCKS_NOW, block is moved."""
        can_push = True
        changed = False
        if not self._can_move(block, direction, flags):
            # only change block direction (but only if block wasn't force moved on this step).
            can_push = False
            if block.step == 0 and (flags & TileWorld.CMM_PUSH_BLOCKS_ALL) and \
                    block.state != ActiveActor.STATE_MOVED:
                block.direction = direction
                changed = True
        elif flags & TileWorld.CMM_PUSH_BLOCKS_ALL:
            # change block direction and move it.
            block.direction = direction
            block.state = ActiveActor.STATE_MOVED
            if flags & TileWorld.CMM_PUSH_BLOCKS_NOW:
                self._perform_move(block, 0)
            changed = True
            can_push = True
            if block.index == self.actor_springing_trap:
                # block on trap button was pushed off, so no one is springing it now.
                self.actor_springing_trap = TileWorld.INDEX_NONE
        if changed:
            self._destroy_moving_actor(block)
        return can_push

    def _choose_move(self, act: MovingActor, teleported: bool) -> None:
        """Choose a move for an actor. The move is stored by changing the actor's direction.
        The teleported flag indicates that the actor was teleported on the previous step."""
        # this will set actor state to MOVED if force move applied.
        self._apply_forced_move(act, teleported)

        if act.entity == Entity.CHIP:
            self._choose_chip_move(act)
            self.chip_last_direction = act.direction

            # save new position for chip, used later for collision checking.
            self.collided_with = TileWorld.INDEX_NONE
            if act.state == ActiveActor.STATE_MOVED:
                self.ticks_since_move = 0
                if not self.flags & TileWorld.FLAG_CHIP_FORCE_MOVED:
                    # note: collision case 1 doesn't apply if chip is subject to a forced move.
                    self.chip_new_pos_x, self.chip_new_pos_y = TileWorld._get_new_actor_pos(act)
            else:
                # update rest timer
                if self.ticks_since_move == TileWorld.CHIP_REST_TICKS:
                    act.direction = TileWorld.CHIP_REST_DIRECTION
                elif self.ticks_since_move < TileWorld.CHIP_REST_TICKS:
                    self.ticks_since_move += 1

        elif not act.entity.is_block():
            # choose monster move
            self._choose_monster_move(act)

        elif act.entity == Entity.BLOCK_GHOST:
            if act.state == ActiveActor.STATE_NONE:
                # ghost block hasn't moved, remove it from actor list without removing the tile.
                # Don't touch ghost blocks that have side effect though.
                tile = self.get_bottom_tile(act.x, act.y)
                if not tile.is_button() and tile != Tile.TRAP:
                    act.state = MovingActor.STATE_GHOST

        # (blocks never move by themselves)

    def _apply_forced_move(self, act: MovingActor, teleported: bool) -> None:
        """If an actor is forced to move in a direction, apply that direction.
        The actor state field must not have been reset since last move when this is called!
        The teleported flag indicates that the actor was teleported on the previous step."""
        if self.current_time == 0:
            return

        tile = self.get_bottom_tile(act.x, act.y)

        if tile.is_ice():
            if act.entity == Entity.CHIP and self.has_ice_boots():
                return
            # continue in same direction
        elif tile.is_slide():
            if act.entity == Entity.CHIP and self.has_slide_boots():
                return
            # take direction of force floor
            act.direction = self._get_slide_dir(tile, True)
        elif not teleported:
            # if teleported, continue in same direction
            # in other cases, move is not forced.
            return

        if act.entity == Entity.CHIP:
            self.flags |= TileWorld.FLAG_CHIP_FORCE_MOVED
        act.state = ActiveActor.STATE_MOVED

    def _choose_chip_move(self, act: MovingActor) -> None:
        """Choose a move for chip given the current input state."""
        state = self.input_state
        assert (state & Direction.VERTICAL_MASK) != Direction.VERTICAL_MASK
        assert (state & Direction.HORIZONTAL_MASK) != Direction.HORIZONTAL_MASK

        if state == 0:
            # no keys pressed
            return

        if self.flags & TileWorld.FLAG_CHIP_FORCE_MOVED and \
                not self.flags & TileWorld.FLAG_CHIP_CAN_UNSLIDE:
            # chip was forced moved and is not allowed to "unslide", skip free choice.
            return

        if (state & Direction.VERTICAL_MASK) != 0 and (state & Direction.HORIZONTAL_MASK) != 0:
            # direction is diagonal
            last_dir = self.chip_last_direction
            last_dir_mask = Direction.to_mask(last_dir)
            if state & last_dir_mask:
                # one of the direction is the current one: continue in current direction, and
                # change direction only if current direction is blocked and other is not.
                other_dir = Direction.from_mask(last_dir_mask ^ state)
                can_move_curr = self._can_move(act, last_dir, TileWorld.CMM_PUSH_BLOCKS)
                can_move_other = self._can_move(act, other_dir, TileWorld.CMM_PUSH_BLOCKS)
                if not can_move_curr and can_move_other:
                    act.direction = other_dir
                else:
                    act.direction = last_dir
            else:
                # neither direction is the current direction: prioritize horizontal movement first.
                if self._can_move(act, Direction.from_mask(state & Direction.HORIZONTAL_MASK),
                                  TileWorld.CMM_PUSH_BLOCKS):
                    state &= Direction.HORIZONTAL_MASK
                else:
                    state &= Direction.VERTICAL_MASK
                act.direction = Direction.from_mask(state)
        else:
            # single direction, apply it.
            act.direction = Direction.from_mask(state)
            # make unused check since it can have side effects (pushing blocks)
            self._can_move(act, act.direction, TileWorld.CMM_PUSH_BLOCKS)

        self.flags |= TileWorld.FLAG_CHIP_SELF_MOVED
        act.state = ActiveActor.STATE_MOVED

    def _choose_monster_move(self, act: MovingActor) -> None:
        """Choose a direction for a monster actor."""
        if act.state == ActiveActor.STATE_MOVED:
            # monster was force moved, do not override
            return

        tile = self.get_bottom_tile(act.x, act.y)
        if tile == Tile.CLONER or tile == tile.TRAP:
            return

        # walker and blob turn is lazy evaluated to avoid changing PRNG state
        WALKER_TURN = 0xff
        BLOB_TURN = 0xfe

        forward = act.direction
        if act.entity == Entity.TEETH:
            # go towards chip
            if (self.current_time + self.stepping) & 0x4:
                # teeth only move at half speed, don't move this time
                return
            x, y = self.current_pos()
            dx = x - act.x
            dy = y - act.y
            choices = []
            if dx < 0:
                choices.append(Direction.WEST)
            elif dx > 0:
                choices.append(Direction.EAST)
            if dy < 0:
                choices.append(Direction.NORTH)
            elif dy > 0:
                choices.append(Direction.SOUTH)
            if abs(dy) >= abs(dx):
                choices = choices[::-1]
        elif act.entity == Entity.BLOB:
            # random direction
            choices = [BLOB_TURN]
        elif act.entity.is_tank():
            choices = [forward]
        elif act.entity == Entity.WALKER:
            # forward and turn randomly if blocked.
            choices = [forward, WALKER_TURN]
        else:
            back = Direction.back(forward)
            if act.entity == Entity.BALL:
                choices = [forward, back]
            else:
                left = Direction.left(forward)
                right = Direction.right(forward)
                if act.entity == Entity.BUG:
                    choices = [left, forward, right, back]
                elif act.entity == Entity.PARAMECIUM:
                    choices = [right, forward, left, back]
                elif act.entity == Entity.GLIDER:
                    choices = [forward, left, right, back]
                else:  # FIREBALL is the only left at this point
                    choices = [forward, right, left, back]

        # attempt move choices in order
        # even if all directions are blocked, still change direction and indicate actor has moved,
        # in case one direction is freed by another actor moving in the meantime.
        act.state = ActiveActor.STATE_MOVED
        for choice in choices:
            if choice == WALKER_TURN:
                choice = (forward - (self._lynx_prng() & 0x3)) % 4
            elif choice == BLOB_TURN:
                cw = [Direction.NORTH, Direction.EAST, Direction.SOUTH, Direction.WEST]
                choice = cw[self._tw_prng() >> 29]
            act.direction = choice
            if self._can_move(act, choice, TileWorld.CMM_CLEAR_ANIM):
                return

        if act.entity == Entity.TEETH:
            # move failed, but still make teeth face chip
            act.direction = choices[0]

    def _perform_move(self, act: MovingActor, flags: int) -> int:
        """Perform chosen move for the given actor. Flags can be provided for use with
        the _can_move method called during this step.
        Returns a result indicating the outcome of the move."""
        if act.step <= 0:
            dir_before = None
            if flags & TileWorld.CMM_RELEASING:
                # if releasing chip from trap, ignore the new direction chosen, use last
                # movement direction. This ensures chip cannot turn while trap is forcing the move.
                if act.entity == Entity.CHIP:
                    dir_before = act.direction
                    act.direction = self.last_chip_direction
            elif act.state == ActiveActor.STATE_NONE:
                # actor has not moved
                return TileWorld.RESULT_SUCCESS

            result = self._start_movement(act, flags)
            if result != TileWorld.RESULT_SUCCESS:
                # there's no need to set state to hidden: actors can only die in start_movement
                # as a result of collision, which ends the game, and we also want to actor tile to
                # be kept to show collision.
                if (flags & TileWorld.CMM_RELEASING) and act.entity == Entity.CHIP:
                    # restore chip chosen direction before releasing from trap
                    act.direction = dir_before
                    self.last_chip_direction = dir_before
                return result

        if not self._continue_movement(act):
            end_cause = self._end_movement(act)
            if end_cause != EndCause.NONE:
                if act.entity == Entity.CHIP:
                    self.end_cause = end_cause
                else:
                    # put actor in the "animated state", with a delay stored in the step field.
                    act.state = MovingActor.STATE_DIED
                    act.step = 11 + ((self.current_time + self.stepping) & 1)
                return TileWorld.RESULT_DIED

        return TileWorld.RESULT_SUCCESS

    def _start_movement(self, act: MovingActor, flags: int) -> int:
        """Initiate a move by an actor. Flags indicate the context from which the move is performed,
        for example releasing an actor from a trap/cloner uses the CMM_RELEASING flag.
        Returns a result indicating the outcome of the move initiation."""
        tile_from = self.get_bottom_tile(act.x, act.y)

        if act.entity == Entity.CHIP:
            if not self.has_slide_boots():
                if tile_from.is_slide() and not (self.flags & TileWorld.FLAG_CHIP_SELF_MOVED):
                    # chip is on force floor and has not moved by himself, award unslide permission.
                    self.flags |= TileWorld.FLAG_CHIP_CAN_UNSLIDE
                elif not tile_from.is_ice() or self.has_ice_boots():
                    # chip is on non force move slide, reclaim unslide permission.
                    self.flags &= ~TileWorld.FLAG_CHIP_CAN_UNSLIDE

            self.flags &= ~(TileWorld.FLAG_CHIP_FORCE_MOVED | TileWorld.FLAG_CHIP_SELF_MOVED)
            self.last_chip_direction = act.direction

        if not self._can_move(act, act.direction,
                              TileWorld.CMM_START_MOVEMENT |
                              TileWorld.CMM_CLEAR_ANIM |
                              TileWorld.CMM_PUSH_BLOCKS_NOW | flags):
            # cannot make chosen move: either another actor made the move before,
            # or move is being forced in blocked direction
            if tile_from.is_ice() and (act.entity != Entity.CHIP or not self.has_ice_boots()):
                act.direction = Direction.back(act.direction)
                self._apply_ice_wall_turn(act)
            return TileWorld.RESULT_FAIL

        if tile_from == Tile.CLONER or tile_from == Tile.TRAP:
            assert flags & TileWorld.CMM_RELEASING

        # check if creature is currently located where chip intends to move (case 1)
        chip_collided = False
        if act.entity.is_monster() and \
                act.x == self.chip_new_pos_x and act.y == self.chip_new_pos_y:
            # collision may occur: chip has moved where a monster was.
            self.collided_with = act.index
            self.collided_actor = act.actor()
        elif act.entity == Entity.CHIP and self.collided_with != TileWorld.INDEX_NONE:
            other = self.actors[self.collided_with]
            if other.state != ActiveActor.STATE_HIDDEN:
                # collision occured and creature has not died in the meantime.
                # this is a special case since the creature has actually moved by this time,
                # so we need to remove it from the tile where it moved to.
                chip_collided = True
                self._set_top_tile(other.x, other.y, Actor.NONE)

        # check if chip is moving on a monster (case 2)
        x, y = TileWorld._get_new_actor_pos(act)
        if act.entity == Entity.CHIP:
            other = self.get_top_tile(x, y)
            if other.entity() != Entity.NONE:
                chip_collided = True
                self.collided_actor = other

        # make move
        if tile_from != Tile.CLONER:
            # (leave actor in cloner if releasing)
            self._set_top_tile(act.x, act.y, Actor.NONE)
        act.x, act.y = x, y
        # the new tile in top layer is set later. This is because direction is stored in top layer
        # and direction may be changed without execution reaching this point (e.g. ice wall turn).

        # check if creature has moved on chip (case 3)
        if act.entity != Entity.CHIP:
            chip = self.get_chip()
            if x == chip.x and y == chip.y:
                chip_collided = True
                self.collided_actor = self.get_top_tile(chip.x, chip.y)
                # if chip has moved, ignore it. This is important because if death occurs, the
                # rest of the tick is processed as usual, but now the actor that has moved on chip
                # took its place and may attempt to move again when chip turn comes.
                # STATE_HIDDEN cannot be used because we want this actor to be shown in collision.
                chip.state = ActiveActor.STATE_NONE

        if chip_collided:
            self.end_cause = EndCause.COLLIDED
            return TileWorld.RESULT_DIED

        act.step += 8

        return TileWorld.RESULT_SUCCESS

    def _continue_movement(self, act: MovingActor) -> bool:
        """Continue an actor's movement. When movement starts, a number of ticks are elapsed
        before the move ends, during which the move is normally animated, but not here.
        For this reason and since the top tile is set in _start_movement, there will be a small
        delay between the move and its side effects (e.g. reaching a key, then picking it up)."""
        assert act.step > 0

        tile_below = self.get_bottom_tile(act.x, act.y)

        speed = 1 if act.entity == Entity.BLOB else 2
        # apply x2 multiplifier on sliding tiles
        if tile_below.is_ice() and (act.entity != Entity.CHIP or not self.has_ice_boots()):
            speed *= 2
        elif tile_below.is_slide() and (act.entity != Entity.CHIP or not self.has_slide_boots()):
            speed *= 2

        act.step -= speed
        return act.step > 0

    def _end_movement(self, act: MovingActor) -> EndCause:
        """Complete the movement for the given actor. Most side effects produced by the move
        occur at this point. An end cause is returned indicating whether the actor survived,
        (=NONE), or otherwise how it died."""
        tile = self.get_bottom_tile(act.x, act.y)
        variant = tile.variant()

        if act.entity != Entity.CHIP or not self.has_ice_boots():
            self._apply_ice_wall_turn(act)

        new_tile = None  # new bottom tile after movement if it changed
        end_cause = EndCause.NONE
        if act.entity == Entity.CHIP:
            if tile == Tile.WATER:
                if not self.has_water_boots():
                    end_cause = EndCause.DROWNED
            elif tile == Tile.FIRE:
                if not self.has_fire_boots():
                    end_cause = EndCause.BURNED
            elif tile == Tile.DIRT or tile == Tile.WALL_BLUE_FAKE:
                new_tile = Tile.FLOOR
            elif tile == Tile.RECESSED_WALL:
                new_tile = Tile.WALL
            elif tile.is_lock():
                if tile != Tile.LOCK_GREEN:
                    self.keys[variant] -= 1
                new_tile = Tile.FLOOR
            elif tile.is_key():
                self.keys[variant] = min(255, self.keys[variant] + 1)
                new_tile = Tile.FLOOR
            elif tile.is_boots():
                self.boots |= 1 << variant  # can use same lookup table as direction
                new_tile = Tile.FLOOR
            elif tile == Tile.THIEF:
                self.boots = 0
            elif tile == Tile.CHIP:
                self.chips_left = max(0, self.chips_left - 1)
                new_tile = Tile.FLOOR
            elif tile == Tile.SOCKET:
                new_tile = Tile.FLOOR
            elif tile == Tile.EXIT:
                end_cause = EndCause.COMPLETE
            elif tile == Tile.HINT:
                if not self.silent:
                    print(f"HINT: {self.level.hint}")
        else:
            # block or monster
            if tile == Tile.WATER:
                if act.entity.is_block():
                    new_tile = Tile.DIRT
                if act.entity != Entity.GLIDER:
                    end_cause = EndCause.DROWNED
            elif tile == Tile.KEY_BLUE:
                # monsters and blocks destroy blue keys
                new_tile = Tile.FLOOR
            # fire is a wall to monsters so they will never end up on it,
            # except fireball and block that survives it

        if tile == Tile.BOMB:
            new_tile = Tile.FLOOR
            end_cause = EndCause.BOMBED
        elif tile == Tile.BUTTON_GREEN:
            self.flags ^= TileWorld.FLAG_TOGGLE_STATE
        elif tile == Tile.BUTTON_BLUE:
            self._turn_tanks(act)
        elif tile == Tile.BUTTON_RED:
            self._activate_cloner(act.x, act.y)

        if new_tile:
            self._set_bottom_tile(act.x, act.y, new_tile)

        return end_cause

    def _turn_tanks(self, trigger: MovingActor) -> None:
        """When a blue button is clicked, all tanks not on ice or a clone machine are marked as
        "reverse tanks", and will be reversed at the end of this step if a blue button wasn't
        clicked again in the meantime by another actor. Depending on the actor list order and the
        presence of ice, there can be some tanks reversed and some not at the end of a step."""
        self.flags |= TileWorld.FLAG_TURN_TANKS
        if trigger.entity.is_tank():
            # there's a moving actor active for this tank, change it directly
            # it will be destroyed later so changing only the tile it's on would reverse the effect.
            trigger.entity = trigger.entity.reverse_tank()
        for i, act in enumerate(self.actors):
            if act.state == ActiveActor.STATE_HIDDEN:
                continue
            tile = self.get_bottom_tile(act.x, act.y)
            if tile == Tile.CLONER or tile.is_ice():
                continue
            act_tile = self.get_top_tile(act.x, act.y)
            if act_tile.entity().is_tank():
                # replace tank by reverse tank or inversely
                self._set_top_tile(act.x, act.y,
                                   act_tile.with_entity(act_tile.entity().reverse_tank()))

    def _find_link(self, x: int, y: int, links: List[Link]) -> Optional[Position]:
        for link in links:
            if link.btn_x == x and link.btn_y == y:
                return link.link_x, link.link_y
        return None

    def _spring_trap(self, x: int, y: int) -> None:
        """Release actor from trap controlled by the button at the given position."""
        pos = self._find_link(x, y, self.level.trap_linkage)
        if not pos:
            # button isn't linked with a trap
            return
        act = self._lookup_actor(*pos)
        if act:
            self._perform_move(act, TileWorld.CMM_RELEASING)
            self._destroy_moving_actor(act)

    def _activate_cloner(self, x: int, y: int) -> None:
        """Release actor from cloner controlled by the button at the given position.
        If maximum number of actors is reached, the parent actor comes out and cloner is empty."""
        pos = self._find_link(x, y, self.level.cloner_linkage)
        if not pos:
            # button isn't linked with a cloner
            return
        parent = self._lookup_actor(*pos)
        if not parent:
            # cloner is empty
            return

        clone = self._spawn_actor()
        if not clone:
            # max number of actors reached, use parent (cloner becomes empty).
            if self._perform_move(parent, TileWorld.CMM_RELEASING) == TileWorld.RESULT_SUCCESS:
                # parent moved successfully, remove it from cloner and persist it.
                self._set_top_tile(pos[0], pos[1], Actor.NONE)
                self._destroy_moving_actor(parent)
            return

        clone.x, clone.y, clone.step, clone.entity, clone.direction, clone.state = \
            parent.x, parent.y, parent.step, parent.entity, parent.direction, parent.state

        parent.state = ActiveActor.STATE_MOVED
        if self._perform_move(parent, TileWorld.CMM_RELEASING) == TileWorld.RESULT_SUCCESS:
            # parent moved successfully out of cloner, persist it.
            self._destroy_moving_actor(parent)
            # clone takes the place of the parent in cloner, persist it.
            self._destroy_moving_actor(clone)
            # if parent move fails, neither is persisted so that parent keeps original position
            # and clone ceases to exist (by virtue of being hidden on spawn)

    def _teleport_actor(self, act: MovingActor) -> None:
        """Teleport an actor on a teleporter to another teleporter, in reverse reading order.
        If all teleporters are blocked in the actor's direction, the actor becomes stuck.
        Returns true if teleport is successful."""
        if act.index == 0 and act.entity != Entity.CHIP:
            # Chip tile was destroyed (see below). Restore it.
            act.entity = self.teleported_chip.entity()
            act.direction = self.teleported_chip.direction()
            self.teleported_chip = Actor.NONE  # only needed for check
        else:
            # If chip tile was destroyed then there's two actors on the same position, don't erase
            # the tile because we'll lose information about the other actor.
            # Otherwise erase the tile to unclaim it, since actor is most likely going to move.
            # This is needed so that the current teleporter appears unclaimed later.
            self._set_top_tile(act.x, act.y, Actor.NONE)

        pos = act.x + act.y * Level.GRID_WIDTH
        orig_pos = pos
        while True:
            pos = (pos - 1) % Level.GRID_SIZE
            px = pos % Level.GRID_WIDTH
            py = pos // Level.GRID_HEIGHT

            if self.get_bottom_tile(px, py) == Tile.TELEPORTER:
                act.x = px
                act.y = py
                if not self.get_top_tile(px, py).entity().is_monster_or_block() \
                        and self._can_move(act, act.direction, 0):
                    # Actor teleported successfully. Its position was changed just before so that
                    # _can_move could be called correctly, keep it there so that the tile gets set
                    # when the actor is destroyed after this call.
                    # Also set the TELEPORTED state to force the move out of the teleporter later.
                    act.state = ActiveActor.STATE_TELEPORTED

                    top_tile = self.get_top_tile(px, py)
                    if top_tile.entity() == Entity.CHIP:
                        # Oops, teleporting on chip (this is legal in TW, not a collision)
                        # Chip tile will be lost after destruction, save it temporarily.
                        # This happens when chip goes in teleporter at the same time or after
                        # a creature, but before that creature moves out of the teleporter.
                        # A bit of a hack, but it only costs 1B of RAM.
                        self.teleported_chip = top_tile
                    return

            if pos == orig_pos:
                # no destination teleporter found, actor is stuck.
                act.x = px
                act.y = py
                if act.entity == Entity.CHIP:
                    self.flags |= TileWorld.FLAG_CHIP_STUCK
                return

    def _get_slide_dir(self, tile: Tile, advance: bool) -> int:
        if tile == Tile.FORCE_FLOOR_RANDOM:
            if advance:
                self.random_slide_dir = Direction.right(self.random_slide_dir)
            direction = self.random_slide_dir
            return direction
        else:
            return tile.variant()

    def _apply_ice_wall_turn(self, act: MovingActor) -> None:
        tile = self.get_bottom_tile(act.x, act.y)
        if tile.is_ice_wall():
            new_dir = TileWorld.ICE_WALL_TURN[act.direction + (tile.variant()) * 4]
            if new_dir is not None:
                act.direction = new_dir

    def _lynx_prng(self) -> int:
        n = ((self.prng_value1 >> 2) - self.prng_value1) & 0xff
        if self.prng_value1 & 0x02 == 0:
            n = (n - 1) & 0xff
        self.prng_value1 = ((self.prng_value1 >> 1) | (self.prng_value2 & 0x80)) & 0xff
        self.prng_value2 = ((self.prng_value2 << 1) | (n & 0x01)) & 0xff
        return self.prng_value1 ^ self.prng_value2

    def _tw_prng(self) -> int:
        self.prng_value0 = ((self.prng_value0 * 1103515245) + 12345) & 0x7FFFFFFF
        return self.prng_value0


class LevelLoader:
    level_pack: Path
    level_count: int
    name: str

    _data: bytes
    _index: List[int]
    _pos: int

    def __init__(self, level_pack: Path):
        self.level_pack = level_pack
        self._pos = 0
        try:
            with open(level_pack, "rb") as file:
                self._data = file.read()
        except IOError as e:
            raise TWException(f"could not read level pack file: {e}")

        signature = self._read(2)
        if signature != 0x5754:
            raise TWException("bad signature")

        self.level_count = self._read(1)
        self._read(1)  # first secret level, skip it
        self._read_index()

    def _read(self, n: int) -> int:
        val = 0
        for i in range(n):
            val |= self._data[self._pos] << (i * 8)
            self._pos += 1
        return val

    def _read_index(self) -> None:
        self._index = []
        self._pos = 4
        addr = 0
        for i in range(self.level_count):
            addr += self._read(2)
            self._index.append(addr)

    def _read_linkage(self, pos: int) -> List[Link]:
        self._pos = pos
        count = self._read(1)
        links = []
        for i in range(count):
            btn_x = self._read(1)
            btn_y = self._read(1)
            link_x = self._read(1)
            link_y = self._read(1)
            links.append(Link(btn_x, btn_y, link_x, link_y))
        return links

    def load(self, level_number: int) -> Level:
        assert level_number > 0
        start_pos = self._index[level_number - 1]
        self._pos = start_pos

        time_limit = self._read(2)
        required_chips = self._read(2)
        layer_data_size = self._read(2)
        password = self._data[self._pos:self._pos + 4].decode("ascii")
        self._pos += 4

        title_pos = start_pos + self._read(2)
        hint_pos = start_pos + self._read(2)
        trap_link_pos = start_pos + self._read(2)
        cloner_link_pos = start_pos + self._read(2)

        grid_data_encoded = self._data[self._pos:self._pos + layer_data_size]
        grid_data = lzss.decode(grid_data_encoded)
        if len(grid_data) != 2 * Level.GRID_SIZE * Level.ENCODED_BITS_PER_TILE // 8:
            raise TWException("bad level data")

        layer_data_raw = int.from_bytes(grid_data, "little")
        layer_data = []
        for i in range(Level.GRID_SIZE * 2):
            layer_data.append(layer_data_raw & Level.ENCODED_TILE_MASK)
            layer_data_raw >>= Level.ENCODED_BITS_PER_TILE

        bottom_layer = []
        top_layer = []
        for i in range(Level.GRID_SIZE):
            bottom_layer.append(Tile(layer_data[i]))
            top_layer.append(Actor(layer_data[Level.GRID_SIZE + i]))

        title = self._data[title_pos:self._data.index(b"\x00", title_pos)].decode("ascii")
        if hint_pos != start_pos:
            hint = self._data[hint_pos:self._data.index(b"\x00", hint_pos)].decode("ascii")
        else:
            hint = ""

        trap_links = self._read_linkage(trap_link_pos)
        cloner_links = self._read_linkage(cloner_link_pos)

        return Level(self.level_pack, level_number, time_limit, required_chips, title, password,
                     hint, trap_links, cloner_links, bottom_layer, top_layer)
