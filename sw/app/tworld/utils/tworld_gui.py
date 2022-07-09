#!/usr/bin/env python3

import argparse
import math
import os
import time
from pathlib import Path
from typing import Optional, Iterator

from make_tileset import TILE_WIDTH_TOP, TILE_WIDTH_BOTTOM, TILE_HEIGHT
from tworld_tws import SolutionLoader

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = "hide"
import pygame

from tworld import TWException, LevelLoader, TileWorld, Level, Direction, EndCause, ActiveActor, \
    Tile, Actor

TILESET_TOP_FILE = "tileset-top.png"
TILESET_BOTTOM_FILE = "tileset-bottom-0.png"

TILE_SIZE = 14


class TileWorldUi:
    level_file: Path
    level_number: int
    full_map: bool

    solution_loader: SolutionLoader

    level: Level
    started: bool
    stepping: int
    start_time: float
    ticks: int
    ticks_since_refresh: int
    state: TileWorld
    input_state: int
    input_since_move: int
    solution_iter: Optional[Iterator[int]]
    solution_step_by_step: bool
    solution_can_step: bool
    solution_last_step: float

    _scr: pygame.Surface
    _tileset_top: pygame.Surface
    _tileset_bottom: pygame.Surface

    TICKS_PER_SECOND = 16  # should be 20, slowed down a bit
    TICKS_PER_REFRESH = 2  # ideally max 2 on gc (will need custom rendering code)
    SMALL_MAP_SIZE = 9

    TICKS_PER_SECOND_SOLUTION = 128
    TICKS_PER_REFRESH_SOLUTION = 4

    def __init__(self, level_file: Path, solution_file: Path,
                 level_number: Optional[int] = None, full_map: bool = False):
        self.level_file = level_file
        self.solution_loader = SolutionLoader(solution_file) if solution_file else None
        self.full_map = full_map
        self.stepping = 0
        if not level_number:
            level_number = 1
        self._go_to_level(level_number)

    def _restart(self) -> None:
        self.ticks = 0
        self.ticks_since_refresh = TileWorldUi.TICKS_PER_REFRESH
        self.state = TileWorld(self.level)
        self.state.stepping = self.stepping
        self.input_state = 0
        self.input_since_move = 0
        self.started = False
        self.solution_iter = None

    def _start(self) -> None:
        if not self.started:
            self.start_time = time.time()
            self.started = True

    def _go_to_level(self, level_number: int) -> None:
        loader = LevelLoader(self.level_file)
        if not (1 <= level_number <= loader.level_count):
            return
        self.level_number = level_number
        self.level = loader.load(level_number)
        self._restart()

    def _step(self) -> None:
        if not self.started:
            self._update_display()
            return

        # calculate number of ticks that occurred since last iteration
        # if more than 4 ticks have occurred, only process 4 to avoid speeding effect.
        new_ticks = math.floor((time.time() - self.start_time) *
                               (TileWorldUi.TICKS_PER_SECOND_SOLUTION if self.solution_iter
                                else TileWorldUi.TICKS_PER_SECOND)) - self.ticks
        self.ticks += new_ticks
        if new_ticks > 4:
            new_ticks = 4

        for i in range(new_ticks):
            if self.solution_iter:
                if self.solution_can_step:
                    # step through solution
                    self.state.step(next(self.solution_iter, 0))
                    self.solution_can_step = not self.solution_step_by_step
            else:
                # step using user input
                state = self.input_state | self.input_since_move
                step_before = self.state.actors[0].step
                self.state.step(state)
                if step_before == 0:
                    self.input_since_move = 0

            self.ticks_since_refresh += 1
            ticks_per_refresh = (TileWorldUi.TICKS_PER_REFRESH_SOLUTION
                                 if self.solution_iter else TileWorldUi.TICKS_PER_REFRESH)
            if self.ticks_since_refresh >= ticks_per_refresh:
                self.ticks_since_refresh = 0
                self._update_display()

            if self.state.is_game_over():
                self._update_display()
                if not self.state.silent:
                    print(f"Game over ({self.state.end_cause.name})")
                    if self.state.end_cause == EndCause.COMPLETE:
                        print(f"Completed in {self.state.current_time} ticks")
                return

    def _show_solution(self) -> None:
        if self.started or not self.solution_loader:
            return

        solution = self.solution_loader.read_solution(self.level_number)
        if not solution:
            print(f"No solution available for level {self.level_number}")
            return

        self.state.silent = True
        self.state.stepping = solution.stepping
        self.state.initial_random_slide_dir = solution.initial_random_slide_dir
        self.state.prng_value0 = solution.prng_seed
        self.state.restart()

        self.solution_iter = iter(solution)
        self._start()

    def _get_key_direction(self, key: int) -> int:
        if key == pygame.K_UP:
            return Direction.NORTH_MASK
        elif key == pygame.K_DOWN:
            return Direction.SOUTH_MASK
        elif key == pygame.K_LEFT:
            return Direction.WEST_MASK
        elif key == pygame.K_RIGHT:
            return Direction.EAST_MASK

    def _handle_input(self) -> bool:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    exit(0)
                elif event.key == pygame.K_r:
                    self._restart()
                elif event.key == pygame.K_SPACE:
                    self._start()
                elif event.key == pygame.K_n:
                    self._go_to_level(self.level_number + 1)
                elif event.key == pygame.K_p:
                    self._go_to_level(self.level_number - 1)
                elif event.key == pygame.K_PAGEUP:
                    self._go_to_level(self.level_number + 10)
                elif event.key == pygame.K_PAGEDOWN:
                    self._go_to_level(self.level_number - 10)
                elif event.key == pygame.K_s:
                    if self.solution_iter:
                        # stop solution and go to manual mode
                        self.solution_iter = None
                        self.state.silent = False
                    else:
                        # start solution
                        self._show_solution()
                        self.solution_step_by_step = bool(pygame.key.get_mods() & pygame.KMOD_SHIFT)
                        self.solution_can_step = not self.solution_step_by_step
                        self.solution_last_step = 0
                elif event.key == pygame.K_t:
                    if self.solution_iter:
                        self.solution_last_step = time.time() + 0.25
                        self.solution_can_step = True
                        self._update_display()
                        print(f"step, time: {self.state.current_time}")
                elif event.key == pygame.K_o:
                    if not self.started:
                        self.stepping = (self.stepping + 1) % 8
                        self.state.stepping = self.stepping
                        print(f"stepping = {self.stepping}")
                else:
                    dir = self._get_key_direction(event.key)
                    if dir:
                        self._start()
                        if (dir & Direction.VERTICAL_MASK) != 0:
                            # if both old and new directions are vertical, ensure only one.
                            if (self.input_state & Direction.VERTICAL_MASK) != 0:
                                self.input_state &= ~Direction.VERTICAL_MASK
                            if (self.input_since_move & Direction.VERTICAL_MASK) != 0:
                                self.input_since_move &= ~Direction.VERTICAL_MASK
                        if (dir & Direction.HORIZONTAL_MASK) != 0:
                            # if both old and new directions are horizontal, ensure only one.
                            if (self.input_state & Direction.HORIZONTAL_MASK) != 0:
                                self.input_state &= ~Direction.HORIZONTAL_MASK
                            if (self.input_since_move & Direction.HORIZONTAL_MASK) != 0:
                                self.input_since_move &= ~Direction.HORIZONTAL_MASK
                        self.input_state |= dir
                        self.input_since_move |= dir
            elif event.type == pygame.KEYUP:
                if event.key == pygame.K_t:
                    self.solution_last_step = 0
                else:
                    dir = self._get_key_direction(event.key)
                    if dir:
                        self.input_state &= ~dir

        if self.solution_iter and self.solution_last_step and \
                time.time() - self.solution_last_step > 1 / TileWorldUi.TICKS_PER_SECOND_SOLUTION:
            self.solution_can_step = True
            self.solution_last_step = time.time()
            print(f"step, time: {self.state.current_time}")

        return True

    def _get_camera_range(self, pos: int) -> range:
        if self.full_map:
            return range(Level.GRID_WIDTH)

        start = pos - math.floor(TileWorldUi.SMALL_MAP_SIZE / 2)
        end = pos + math.ceil(TileWorldUi.SMALL_MAP_SIZE / 2)
        if start < 0:
            start = 0
            end = TileWorldUi.SMALL_MAP_SIZE
        elif end > Level.GRID_WIDTH:
            start = Level.GRID_WIDTH - TileWorldUi.SMALL_MAP_SIZE
            end = Level.GRID_WIDTH
        return range(start, end)

    def _update_title(self) -> None:
        keys = (("BRGY"[i] if c > 0 else " ") for i, c in enumerate(self.state.keys))
        boots = (("WFIS"[i] if self.state.boots & (1 << i) else " ") for i in range(4))
        time_left = self.level.time_limit - self.state.current_time
        time_left_str = "" if self.level.time_limit == 0 else \
            f" - {math.ceil(time_left / TileWorld.TICKS_PER_SECOND)} s"
        if self.full_map:
            actor_count = sum(1 for act in self.state.actors
                              if act.state != ActiveActor.STATE_HIDDEN)
            title = (f"Tile World - Level {self.level_number}{time_left_str} "
                     f"- {self.state.chips_left} chips needed "
                     f"- keys [{'] ['.join(keys)}] "
                     f"- boots [{'] ['.join(boots)}] ({actor_count})")
        else:
            title = (f"TW{self.level_number}{time_left_str} "
                     f"- {self.state.chips_left} chips "
                     f"- keys [{'] ['.join(keys)}] "
                     f"- boots [{'] ['.join(boots)}] ")
        pygame.display.set_caption(title)

    def _draw_bottom_tile(self, buf: pygame.Surface, x: int, y: int, tile: Tile) -> None:
        value = tile.value
        sxb = (value // 8) * TILE_WIDTH_BOTTOM
        syb = (value % 8) * TILE_HEIGHT
        buf.blit(self._tileset_bottom, (x * TILE_SIZE, y * TILE_SIZE),
                 (sxb, syb, TILE_WIDTH_BOTTOM, TILE_HEIGHT))

    def _draw_top_tile(self, buf: pygame.Surface, x: int, y: int, actor: Actor) -> None:
        if actor.is_block():
            self._draw_bottom_tile(buf, x, y, Tile.BLOCK)
            return
        value = actor.value
        sxb = (value // 8) * TILE_WIDTH_TOP
        syb = (value % 8) * TILE_HEIGHT
        buf.blit(self._tileset_top, (x * TILE_SIZE + 1, y * TILE_SIZE),
                 (sxb, syb, TILE_WIDTH_TOP, TILE_HEIGHT))

    def _update_display(self) -> None:
        self._update_title()

        pos_x, pos_y = self.state.current_pos()
        xrange = self._get_camera_range(pos_x)
        yrange = self._get_camera_range(pos_y)

        frame_buf = pygame.Surface((len(xrange) * TILE_SIZE, len(yrange) * TILE_SIZE))

        # bottom layer
        for y in yrange:
            for x in xrange:
                px = x - xrange.start
                py = y - yrange.start
                bottom_tile = self.state.get_bottom_tile(x, y)
                self._draw_bottom_tile(frame_buf, px, py, bottom_tile)

        # if collision occured, draw collided with actor below
        # (since actors are on top layer there cannot be 2 actors on one tile otherwise)
        if self.state.end_cause == EndCause.COLLIDED:
            px, py = self.state.current_pos()
            px -= xrange.start
            py -= yrange.start
            self._draw_top_tile(frame_buf, px, py, self.state.collided_actor)

        # top layer
        for y in yrange:
            for x in xrange:
                px = x - xrange.start
                py = y - yrange.start
                top_tile = self.state.get_top_tile(x, y)
                if top_tile != Actor.NONE:
                    self._draw_top_tile(frame_buf, px, py, top_tile)

        frame_buf = pygame.transform.scale(frame_buf,
                                           (self._scr.get_width(), self._scr.get_height()))
        self._scr.blit(frame_buf, (0, 0))
        pygame.display.flip()

    def _init_window(self) -> None:
        pygame.init()
        self._tileset_top = pygame.image.load(TILESET_TOP_FILE)
        self._tileset_bottom = pygame.image.load(TILESET_BOTTOM_FILE)
        scr_size = TILE_SIZE * ((32 * 2) if self.full_map else (9 * 4))
        self._scr = pygame.display.set_mode((scr_size, scr_size))
        pygame.display.set_caption("Tile World")

    def run(self) -> None:
        self._init_window()
        self._restart()
        while True:
            if not self._handle_input():
                break
            if not self.state.is_game_over():
                self._step()

        pygame.quit()


def main() -> None:
    args = parser.parse_args()

    # load level from level pack file
    level_spec = args.level.split(":")
    if len(level_spec) > 2:
        raise TWException("unexpected level format")
    if len(level_spec) == 1:
        level_number = None
    else:
        try:
            level_number = int(level_spec[1])
        except ValueError:
            raise TWException("bad level number")
    level_file = Path(level_spec[0])
    if not level_file.exists():
        raise TWException("level pack doesn't exist")

    solution_file = None
    if args.solution:
        solution_file = Path(args.solution)
        if not solution_file.exists():
            raise TWException("solution file doesn't exist")

    # start game
    ui = TileWorldUi(level_file, solution_file, level_number, args.full_map)
    ui.run()


parser = argparse.ArgumentParser(description="Tile world reference implementation")
parser.add_argument(
    "level", action="store", type=str,
    help="Level to load, in the format <level pack filename>:<level number>."
         "If level is not specified, level 1 is used.")
parser.add_argument(
    "-f", "--full-map", action="store_true", dest="full_map",
    help="Show full map instead of player restricted view")
parser.add_argument(
    "-s", "--solution", action="store", type=str, dest="solution",
    help="TWS solution file for level pack used for showing solutions.")

if __name__ == '__main__':
    try:
        main()
    except TWException as e:
        print(f"ERROR: {e}")
        exit(1)
