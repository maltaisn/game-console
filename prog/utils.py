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
from typing import Callable

ProgressCallback = Callable[[int, int], None]


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
