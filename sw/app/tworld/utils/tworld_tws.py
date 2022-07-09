from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Generator

from tworld import TWException, Direction


@dataclass(frozen=True)
class Move:
    delta: int
    direction: int


@dataclass(frozen=True)
class Solution:
    total_time: int
    stepping: int
    initial_random_slide_dir: int
    prng_seed: int
    moves: List[Move]

    def __iter__(self) -> Generator[int, None, None]:
        index = 0
        time = 0
        while True:
            if time >= self.moves[index].delta:
                time -= self.moves[index].delta
                yield self.moves[index].direction
                index += 1
                if index == len(self.moves):
                    return
            else:
                yield 0

            time += 1


class SolutionLoader:
    _data: bytes
    _pos: int

    DIRECTIONS = [
        Direction.NORTH_MASK,
        Direction.WEST_MASK,
        Direction.SOUTH_MASK,
        Direction.EAST_MASK,
        Direction.NORTH_MASK | Direction.WEST_MASK,
        Direction.SOUTH_MASK | Direction.WEST_MASK,
        Direction.NORTH_MASK | Direction.EAST_MASK,
        Direction.SOUTH_MASK | Direction.EAST_MASK,
    ]

    def __init__(self, filename: Path):
        with open(filename, "rb") as file:
            self._data = file.read()

        self._pos = 0
        if self._data[0:4] != b"\x35\x33\x9b\x99":
            raise TWException("bad TWS signature")
        if self._data[4] != 1:
            raise TWException("only Lynx ruleset supported")

    def _read(self, n: int) -> int:
        val = 0
        for i in range(n):
            val |= self._data[self._pos] << (i * 8)
            self._pos += 1
        return val

    def _read_move_type1(self, length: int) -> List[Move]:
        move = self._read(length)
        direction = SolutionLoader.DIRECTIONS[(move >> 2) & 0x7]
        delta = (move >> 5) + 1
        return [Move(delta, direction)]

    def _read_move_type2(self) -> List[Move]:
        move = self._read(4)
        direction = SolutionLoader.DIRECTIONS[(move >> 2) & 0x3]
        delta = ((move >> 5) + 1) & 0x7fffff
        return [Move(delta, direction)]

    def _read_move_type3(self) -> List[Move]:
        move = self._read(1)
        direction0 = SolutionLoader.DIRECTIONS[(move >> 2) & 0x3]
        direction1 = SolutionLoader.DIRECTIONS[(move >> 4) & 0x3]
        direction2 = SolutionLoader.DIRECTIONS[(move >> 6) & 0x3]
        return [Move(4, direction0), Move(4, direction1), Move(4, direction2)]

    def _read_move_type4(self, length: int) -> List[Move]:
        move = self._read(length)
        direction = (move >> 5) & 0x1ff
        if direction not in SolutionLoader.DIRECTIONS:
            raise TWException("unsupported type 4 move encoding (mouse)")
        delta = (move >> 14) + 1
        return [Move(delta, direction)]

    def read_solution(self, level_number: int) -> Optional[Solution]:
        assert level_number > 0
        self._pos = 8
        while self._pos < len(self._data):
            offset = self._read(4)
            if offset == 0:
                continue  # padding
            end_pos = self._pos + offset
            number = self._read(2)
            if number == level_number:
                break
            self._pos = end_pos
        else:
            return None

        self._pos += 5
        initial_conditions = self._read(1)
        initial_random_slide_dir = initial_conditions & 0x7
        stepping = (initial_conditions >> 3) & 0x7

        prng_seed = self._read(4)
        total_time = self._read(4)
        all_moves = []
        while self._pos < end_pos:
            moves: List[Move]
            b = self._data[self._pos]
            if b & 0x3 == 0b00:
                moves = self._read_move_type3()
            elif b & 0x3 == 0b01:
                moves = self._read_move_type1(1)
            elif b & 0x3 == 0b10:
                moves = self._read_move_type1(2)
            elif b & 0x10 == 0x10:
                moves = self._read_move_type4(((b >> 2) & 0x3) + 2)
            elif b & 0x10 == 0x00:
                moves = self._read_move_type2()
            else:
                raise TWException("unknown move encoding")

            if not all_moves:
                # first move has -1 on delta
                moves[0] = Move(moves[0].delta - 1, moves[0].direction)
            all_moves += moves

        if self._pos != end_pos:
            raise TWException("truncated move encoding")

        return Solution(total_time, stepping, initial_random_slide_dir, prng_seed, all_moves)
