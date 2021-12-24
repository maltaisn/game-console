#  Copyright 2021 Nicolas Maltais
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
from typing import Callable

ProgressCallback = Callable[[int, int], None]


def readable_size(size: int, decimals: int = 2) -> str:
    """Print size in human readable format, with units."""
    precision = 10 ** decimals
    if size < 1024:
        return f"{size} B"
    else:
        for unit in ["", "k", "M", "G", "T"]:
            if abs(size) < 1024:
                return f"{round(size * precision) / precision} {unit}B"
            size /= 1024
        raise ValueError()


def print_progress_bar(name: str, left_space: int, width: int) -> ProgressCallback:
    """Print a progress bar of a certain width with a name padded to a
    number of columns (left_space). Returns a callback to use to update progress bar."""
    start = time.time()

    def callback(current: int, total: int) -> None:
        progress = 1 if total == 0 else (current / total)
        n = math.floor(progress * width)
        print("\033[2K", end="")  # erase previous bar
        print(f"{name.ljust(left_space, ' ')} | {('#' * n)}{' ' * (width - n)} | "
              f"{f'{progress:.0%},':<4} {f'{time.time() - start:.2f} s,':<7} "
              f"{readable_size(current)} / {readable_size(total)}\r", end="")
        if current == total:
            print()

    return callback
