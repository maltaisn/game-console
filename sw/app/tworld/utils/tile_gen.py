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
import itertools
import math
from dataclasses import dataclass
from pathlib import Path

from PIL import Image
from assets.types import DataObject, PackResult, PackError

TILE_HEIGHT = 14
TILE_WIDTH_TOP = 12
TILE_WIDTH_BOTTOM = 16
TILE_PADDING_BOTTOM = 1

# bit position for alpha bits and color nibbles in top tiles (total size = 64 bits)
TOP_COLOR_POS = [8,  12, 16, 20, 24, 28, 40, 44, 48, 52, 56, 60]
TOP_ALPHA_POS = [0,  1,  2,  3,  4,  5,  32, 33, 34, 35, 36, 37]


def color_4bit(value: int) -> int:
    return math.floor((value / 256) * 16)


@dataclass(frozen=True)
class TileObject(DataObject):
    image: Image
    alpha: bool

    def pack(self) -> PackResult:
        data = bytearray()
        for y in range(TILE_HEIGHT):
            row = 0
            if self.alpha:
                nibbles = TILE_WIDTH_TOP + 4  # +2 bytes for alpha
                for x in range(TILE_WIDTH_TOP):
                    color = self.image.getpixel((x, y))
                    if color[1] > 128:
                        row |= 1 << TOP_ALPHA_POS[x]
                    row |= color_4bit(color[0]) << TOP_COLOR_POS[x]
            else:
                nibbles = TILE_WIDTH_BOTTOM
                for x in range(TILE_WIDTH_BOTTOM):
                    px = x - TILE_PADDING_BOTTOM
                    color = self.image.getpixel((px, y)) \
                        if 0 <= px < TILE_WIDTH_BOTTOM - 2 * TILE_PADDING_BOTTOM else 0
                    row |= color_4bit(color) << (x * 4)

            data += row.to_bytes(nibbles // 2, "little")

        return PackResult(data)

    def get_type_name(self) -> str:
        return "tile image"


def register_builder(packer) -> None:
    @packer.file_builder
    def tileset(filename: Path, *, width: int, height: int, tile_width: int, alpha: bool = False):
        img = Image.open(filename)
        if img.size != (width * tile_width, height * TILE_HEIGHT):
            raise PackError("invalid tileset image size")
        img = img.convert("LA" if alpha else "L")

        # Create a data object for each tile in the tileset by cropping the image.
        # Also create a map to deduplicate the tiles. Each index in the map is a tile ID and
        # the value is the position in the generated tile objects array.
        map = {}
        map_flat = []
        pos = 0
        for x in range(width):
            for y in range(height):
                tile_img = img.crop((x * tile_width, y * TILE_HEIGHT,
                                     (x + 1) * tile_width, (y + 1) * TILE_HEIGHT))

                data = tile_img.getdata()
                if alpha:
                    data = itertools.chain(*data)  # flatten, 2 bytes per pixel
                data = bytes(data)
                if all(b == 0 for b in data):
                    # fully black/transparent tile, treat as undefined.
                    map_flat.append(0)
                    continue

                if data not in map:
                    map[data] = pos
                    yield TileObject(tile_img, alpha)
                    pos += 1
                map_flat.append(map[data])

        return map_flat
