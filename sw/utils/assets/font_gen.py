#!/usr/bin/env python3

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

# Usage:
#
#     ./font_gen.py --help
#     ./font_gen.py <input_file> [output_file] -W <glyph-width> -H <glyph-height>
#
# Y offsets will be deduced from image height and data. Image must be 8-bit gray PNG
# with only black and white colors.
# Char ranges 0x20-0x7f, then 0xa0-0xff are located contiguously on image, with each
# char separated by one pixel of whitespace.

import argparse
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List

from PIL import Image

sys.path.append(str(Path(__file__).absolute().parent.parent))  # for standalone run
from assets.types import PackResult, DataObject, PackError

STD_IO = "-"

parser = argparse.ArgumentParser(description="Font generation utility for game console")
parser.add_argument(
    "input_file", action="store", type=str,
    help="Input PNG file. Must be 8-bit gray with only black or white colors.")
parser.add_argument(
    "output_file", action="store", type=str, default=STD_IO, nargs="?",
    help="Output file. Can be set to '-' for standard output (default).")
parser.add_argument(
    "-W", "--width", action="store", type=int, dest="glyph_width", required=True,
    help="Glyph width on input file in pixels")
parser.add_argument(
    "-H", "--height", action="store", type=int, dest="glyph_height", required=True,
    help="Glyph height on input file in pixels")
parser.add_argument(
    "-s", "--line-spacing", action="store", type=int, dest="line_spacing", default=1,
    help="Extra line spacing in pixels (added to glyph height, default is 1)")


def range_repr(r: range) -> str:
    return f"{r.start}-{r.stop - 1}"


@dataclass
class GlyphData:
    pos: int
    data: int
    offset: int

    def encode(self, bit_length: int, offset_bits: int, nb_bytes: int) -> bytes:
        data = self.data
        # append data after the current highest bit
        data |= self.offset << (bit_length - offset_bits)
        # first bit read will be MSB, so shift to byte boundary to fill the gap
        # (e.g. shift by 1 if glyph is 15 bits encoded on 2 bytes)
        data <<= nb_bytes * 8 - bit_length
        return data.to_bytes(nb_bytes, "little", signed=False)


class EncodeError(Exception):
    pass


@dataclass
class FontData:
    """A collection of glyph data representing a font. The font has glyph size, Y offset bits,
    and bytes per glyph attributes."""
    width: int
    height: int
    line_spacing: int
    glyphs: List[GlyphData] = field(default_factory=list, repr=False)
    bytes_per_glyph: int = field(default=0)
    max_offset: int = field(default=-1)
    offset_bits: int = field(default=0)

    SIGNATURE = 0xf0

    GLYPH_WIDTH_RANGE = range(1, 17)
    GLYPH_HEIGHT_RANGE = range(1, 17)
    MAX_OFFSET_RANGE = range(0, 16)
    LINE_SPACING_RANGE = range(0, 33)
    BYTES_PER_GLYPH_RANGE = range(1, 34)

    RANGES = [range(0x21, 0x80), range(0xa0, 0x100)]
    MAX_GLYPHS = sum((len(r) for r in RANGES))

    def encode(self) -> bytes:
        count = len(self.glyphs)
        if count > FontData.MAX_GLYPHS:
            raise EncodeError(f"maximum {FontData.MAX_GLYPHS} glyphs allowed, got {count}.")

        self.max_offset = max(self.glyphs, key=lambda g: g.offset).offset
        if self.max_offset not in FontData.MAX_OFFSET_RANGE:
            raise EncodeError(f"Y offset of {self.max_offset} px is out of bounds, "
                              f"valid range is {range_repr(FontData.MAX_OFFSET_RANGE)}")
        self.offset_bits = 0 if self.max_offset == 0 else int(math.log2(self.max_offset) + 1)

        glyph_bit_length = self.width * self.height + self.offset_bits
        self.bytes_per_glyph = ((glyph_bit_length - 1) // 8) + 1
        if self.bytes_per_glyph not in FontData.BYTES_PER_GLYPH_RANGE:
            raise EncodeError(f"font data glyph size of {self.bytes_per_glyph} bytes is out of "
                              f"bounds, valid range is {range_repr(FontData.BYTES_PER_GLYPH_RANGE)}")

        data = bytearray()
        data.append(FontData.SIGNATURE)
        data.append(count)
        data.append(self.bytes_per_glyph)
        data.append((self.width - 1) | (self.height - 1) << 4)
        data.append(self.offset_bits | self.max_offset << 4)
        data.append(self.line_spacing)
        for glyph in self.glyphs:
            data += glyph.encode(glyph_bit_length, self.offset_bits, self.bytes_per_glyph)
        return data


@dataclass
class Config:
    input_file: Path
    output_file: str
    glyph_width: int
    glyph_height: int
    line_spacing: int
    verbose: bool


@dataclass(frozen=True)
class FontPackResult(PackResult):
    font_data: FontData

    def __repr__(self) -> str:
        d = self.font_data
        glyph_count = len(d.glyphs)
        s = super().__repr__()
        s += f", {glyph_count} glyphs, {d.bytes_per_glyph} bytes per glyph,\n" \
             f"{d.offset_bits} offset bits (max offset is {d.max_offset} px), encode ranges "
        total = 0
        for r in FontData.RANGES:
            if total + len(r) > glyph_count:
                s += f"{r.start:#02x}-{r.start + (glyph_count - total - 1):#02x}, "
                break
            else:
                s += f"{r.start:#02x}-{r.stop - 1:#02x}, "
                total += len(r)
        return s[:-2]


@dataclass(frozen=True)
class FontObject(DataObject):
    file: Path
    glyph_width: int
    glyph_height: int
    line_spacing: int

    def pack(self) -> PackResult:
        config = Config(self.file, "", self.glyph_width, self.glyph_height,
                        self.line_spacing, False)
        try:
            font_data = create_font_data(config)
            data = font_data.encode()
        except EncodeError as e:
            raise PackError(f"encoding error: {e}")
        return FontPackResult(data, font_data)

    def is_in_unified_data_space(self) -> bool:
        return True

    def get_type_name(self) -> str:
        return "font"


def register_builder(packer) -> None:
    @packer.file_builder
    def font(filename: Path, *, glyph_width: int, glyph_height: int,
             extra_line_spacing: int = 1):
        yield FontObject(filename, glyph_width, glyph_height,
                         glyph_height + extra_line_spacing)


def create_config(args: argparse.Namespace) -> Config:
    input_file = Path(args.input_file)
    if not input_file.exists() or not input_file.is_file():
        raise EncodeError("invalid input file")

    line_spacing: int = args.line_spacing + args.glyph_height

    output_file = args.output_file
    verbose = output_file != STD_IO

    return Config(input_file, output_file, args.glyph_width, args.glyph_height,
                  line_spacing, verbose)


def create_font_data(config: Config) -> FontData:
    """Read PNG image and create font data from it."""
    if config.glyph_width not in FontData.GLYPH_WIDTH_RANGE:
        raise EncodeError(f"glyph width out of bounds "
                          f"(valid range is {range_repr(FontData.GLYPH_WIDTH_RANGE)})")
    if config.glyph_height not in FontData.GLYPH_HEIGHT_RANGE:
        raise EncodeError(f"glyph height out of bounds "
                          f"(valid range is {range_repr(FontData.GLYPH_HEIGHT_RANGE)})")

    if config.line_spacing not in FontData.LINE_SPACING_RANGE:
        raise EncodeError(f"line spacing out of bounds "
                          f"({config.line_spacing}+{config.glyph_height}={config.line_spacing}, "
                          f"valid range is {range_repr(FontData.LINE_SPACING_RANGE)})")

    image = Image.open(config.input_file)
    width, height = image.size

    if not isinstance(image.getpixel((0, 0)), int):
        raise EncodeError("image is not 8-bit gray PNG")

    if config.glyph_height > height:
        raise EncodeError(f"glyph height is larger than image height")

    font_data = FontData(config.glyph_width, config.glyph_height, config.line_spacing)
    for x in range(0, width, config.glyph_width + 1):
        glyph = read_glyph(image, x, config.glyph_width, config.glyph_height)
        font_data.glyphs.append(glyph)

    return font_data


def read_glyph(image: Image, pos: int, width: int, height: int) -> GlyphData:
    """Create glyph data from a position in an image."""
    _, im_height = image.size

    # find Y offset by finding first set pixel from bottom up
    offset = 0
    for y in range(im_height - 1, height - 1, -1):
        for x in range(pos, pos + width):
            if image.getpixel((x, y)) == 0:
                offset = y - height + 1
                break
        if offset != 0:
            break

    # encode glyph data
    # - Y coordinate goes down with image.getpixel
    # - data is read from byte with highest address to byte with lowest address,
    #   and from most significant bit to least significant bit.
    #   The first bit read is the one at the top left of the glyph.
    data = 0
    length = 0
    for y in range(0, height + offset):
        for x in range(pos, pos + width):
            pixel = image.getpixel((x, y))
            if pixel != 0 and pixel != 255:
                raise EncodeError("image is not black and white")
            pixel_set = (pixel == 0)
            if y >= offset:
                data = data << 1 | pixel_set
                length += 1
            elif pixel_set:
                raise EncodeError(f"glyph exceeds height at x={pos}")

    # check if glyph doesn't exceed width
    for y in range(0, im_height):
        if image.getpixel((pos + width, y)) == 0:
            raise EncodeError(f"glyph exceeds width at x={pos}")

    return GlyphData(pos, data, offset)


def main():
    args = parser.parse_args()
    config = create_config(args)
    font_data = create_font_data(config)
    data = font_data.encode()

    # show information on generated font
    glyph_count = len(font_data.glyphs)
    if config.verbose:
        print(f"Done, total size is {len(data)} bytes, "
              f"{font_data.bytes_per_glyph} bytes per glyph, "
              f"{font_data.offset_bits} offset bits (max offset is {font_data.max_offset} px).")
        total = 0
        ranges_str = []
        for r in FontData.RANGES:
            if total + len(r) > glyph_count:
                ranges_str.append(
                    f"{r.start:#02x}-{r.start + (glyph_count - total - 1):#02x}")
                break
            else:
                ranges_str.append(f"{r.start:#02x}-{r.stop - 1:#02x}")
                total += len(r)
        print(f"Font encodes ranges {', '.join(ranges_str)} ({glyph_count} chars)")

    try:
        if config.output_file == STD_IO:
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
        else:
            with open(config.output_file, "wb") as file:
                file.write(data)
    except IOError as e:
        raise EncodeError(f"could not write to output file: {e}") from e


if __name__ == '__main__':
    try:
        main()
    except EncodeError as ex:
        print(f"ERROR: {ex}", file=sys.stderr)
        exit(1)
