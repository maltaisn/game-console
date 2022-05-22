#  Copyright 2022 Nicolas Maltais
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import math
import time
from pathlib import Path
from typing import Callable, Union, Any

# A function that takes into argument a current position and a total,
# the progress is the ratio of the two.
from bitarray import bitarray

ProgressCallback = Callable[[int, int], None]

PathLike = Union[str, Path]

PROGRESS_BAR_WIDTH = 30
PROGRESS_BAR_MARGIN = 10


class DataReader:
    data: bytes
    pos: int

    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read(self, n: int) -> int:
        value = 0
        for j in range(n):
            value |= self.data[self.pos] << (j * 8)
            self.pos += 1
        return value


class DataWriter:
    data: bytearray

    def __init__(self):
        self.data = bytearray()

    def write(self, value: int, n: int) -> None:
        for j in range(n):
            self.data.append(value & 0xff)
            value >>= 8


def no_progress(c, t) -> None:
    pass


def parse_dec_or_hex_number(s: str) -> int:
    s = s.strip()
    if s.startswith("0x"):
        return int(s, 16)
    else:
        return int(s, 10)


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


def progress_bar(left: str, right: str, progress: float) -> str:
    n = math.floor(progress * PROGRESS_BAR_WIDTH)
    if progress > 0 and n == 0:
        n = 1
    return (f"{left.ljust(PROGRESS_BAR_MARGIN, ' ')} "
            f"[{('#' * n)}{' ' * (PROGRESS_BAR_WIDTH - n - 1)}] "
            f"{f'{round(progress * 1000) / 10:.1f}%,':<8}{right}")


def print_progress_bar(name: str, right: str = "",
                       transform: Callable[[int], Any] = readable_size,
                       verbose: bool = True) -> ProgressCallback:
    """Print a progress bar of a certain width with a name padded to a
    number of columns (left_space). Returns a callback to use to update progress bar."""
    if not verbose:
        return no_progress

    start = time.time()

    def callback(current: int, total: int) -> None:
        progress = 1 if total == 0 else (current / total)
        print("\033[2K", end="")  # erase previous bar
        info = (f"{f'{time.time() - start:.1f} s,':<8} "  # time
                f"{transform(current)} / {transform(total)}{right}")  # curr & total size
        print(progress_bar(name, info, progress), end="\r")
        if current == total:
            print()

    return callback


def process_bitarray_sequences(driver, mask: bitarray, name: str,
                               func: Callable[[int, int, ProgressCallback], None],
                               verbose: bool = True) -> None:
    """Identify continuous sequences of ones in the bitarray representing blocks in a memory
    device, and call `func` for each of these sequences, tracking overall progress."""
    block_size = driver.get_smallest_erase_size()
    block_count = driver.get_block_count()
    mem_size = driver.get_size()
    progress = print_progress_bar(name, verbose=verbose)
    start = 0
    total_bytes = 0
    total_to_process = mask.count() * block_size
    while start < mem_size:
        while not mask[start]:
            start += 1
            if start == block_count:
                return
        end = start
        while mask[end]:
            end += 1
            if end == block_count:
                break
        start_addr = start * block_size
        end_addr = end * block_size
        func(start_addr, end_addr, lambda c, t: progress(total_bytes + c, total_to_process))
        total_bytes += end_addr - start_addr
        start = end
