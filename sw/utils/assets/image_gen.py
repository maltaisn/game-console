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
#     ./image_gen.py --help
#     ./image_gen.py <input_file> [output_file]
#
import abc
import argparse
import math
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional, Generator, Callable, Tuple

from PIL import Image

from assets.types import PackResult, DataObject, PackError

STD_IO = "-"


class EncodeError(Exception):
    pass


class IndexGranularityMode(Enum):
    ROW_COUNT = 0
    MAX_SIZE = 1


@dataclass
class IndexGranularity:
    mode: IndexGranularityMode
    value: int

    @staticmethod
    def from_str(s: str) -> "IndexGranularity":
        mode: IndexGranularityMode
        if s[-1] == 'b':
            mode = IndexGranularityMode.MAX_SIZE
        elif s[-1] == 'r':
            mode = IndexGranularityMode.ROW_COUNT
        else:
            raise ValueError("invalid mode")
        value = int(s[:-1])
        if mode == IndexGranularityMode.MAX_SIZE and \
                not (0 < value <= ImageIndex.MAX_ENTRY) or \
                mode == IndexGranularityMode.ROW_COUNT and \
                not (0 < value < ImageEncoder.MAX_IMAGE_SIZE):
            raise ValueError("value out of bounds")

        return IndexGranularity(mode, value)

    def __repr__(self):
        return f"{self.value}{'b' if self.mode == IndexGranularityMode.MAX_SIZE else 'r'}"


@dataclass
class Rect:
    left: int
    top: int
    right: int
    bottom: int

    def width(self) -> int:
        return self.right - self.left + 1

    def height(self) -> int:
        return self.bottom - self.top + 1


TRANSPARENT_COLOR = 0xff


def pixel_iterator(image: Image, levels: int, rect: Optional[Rect] = None,
                   row_cb: Optional[Callable[[int], None]] = None) -> Generator[int, None, None]:
    if not rect:
        rect = Rect(0, 0, image.width - 1, image.height - 1)
    for y in range(rect.top, rect.bottom + 1):
        if row_cb:
            row_cb(y)
        for x in range(rect.left, rect.right + 1):
            color, alpha = image.getpixel((x, y))
            if alpha < 128:
                yield TRANSPARENT_COLOR
            else:
                yield math.floor((color / 256) * levels)


@dataclass
class ImageIndex:
    granularity: int
    entries: List[int] = field(default_factory=list)

    MAX_ENTRY = 256

    def encode(self) -> bytes:
        data = bytearray([self.granularity])
        for entry in self.entries:
            if not (0 < entry <= ImageIndex.MAX_ENTRY):
                raise ValueError("Cannot encode index, out of bounds")
            data.append(entry - 1)
        return data


@dataclass
class ImageData:
    flags: int
    alpha_color: int
    width: int
    height: int
    index: Optional[ImageIndex]
    data: bytes

    SIGNATURE = 0xf1

    FLAG_BINARY = 1 << 4
    FLAG_RAW = 1 << 5
    FLAG_ALPHA = 1 << 6
    FLAG_INDEXED = 1 << 7

    def encode(self) -> bytes:
        alpha_color = self.alpha_color if self.flags & ImageData.FLAG_ALPHA else 0
        data = bytearray()
        data.append(ImageData.SIGNATURE)
        data.append(self.flags & 0xf0 | alpha_color & 0xf)
        data.append(self.width - 1)
        data.append(self.height - 1)
        if self.flags & ImageData.FLAG_INDEXED:
            data += self.index.encode()
        data += self.data
        return data


@dataclass
class ImageEncoding:
    # whether the encoding is 1-bit or 4-bit, or best choice (=None)
    binary: Optional[bool]
    # whether the encoding is raw or mixed
    raw: bool


class ImageEncoder(abc.ABC):
    image: Image
    region: Rect
    alpha_color: int
    indexed: bool
    index_granularity: IndexGranularity
    index: Optional[ImageIndex]

    _indexed: bool
    _data: bytearray
    _flags: int
    _run_length: int
    _last_color: int
    _last_data_len: int

    MAX_IMAGE_SIZE = 256

    DEFAULT_INDEX_GRANULARITY = IndexGranularity(IndexGranularityMode.MAX_SIZE, 64)

    # used to ignore alpha color and treat it as black.
    ALPHA_COLOR_NONE = -1

    _LAST_COLOR_NONE = -1

    def __init__(self, image: Image):
        self._flags = 0
        self.image = image
        self.region = Rect(0, 0, image.width - 1, image.height - 1)
        self.indexed = True
        self.index_granularity = ImageEncoder.DEFAULT_INDEX_GRANULARITY
        self.alpha_color = ImageEncoder.ALPHA_COLOR_NONE
        self._reset()

    def _reset(self) -> None:
        self.index = None
        self._indexed = self.indexed
        self._data = bytearray()
        self._last_data_len = 0
        self._run_length = 0
        self._last_color = ImageEncoder._LAST_COLOR_NONE

    def _iterate_pixels(self) -> None:
        def row_cb(y: int) -> None:
            if y % self.index.granularity == 0 and self._indexed:
                self._end_run_length(0, True)
                self.index.entries.append(len(self._data) - self._last_data_len)
                self._last_data_len = len(self._data)

        for color in pixel_iterator(self.image, self.get_gray_levels(), self.region, row_cb):
            if color == TRANSPARENT_COLOR:
                if self.alpha_color == ImageEncoder.ALPHA_COLOR_NONE:
                    color = 0
                else:
                    self._flags |= ImageData.FLAG_ALPHA
                    color = self.alpha_color

            if (self._last_color == ImageEncoder._LAST_COLOR_NONE or color == self._last_color) \
                    and self._run_length < self._get_max_run_length():
                self._run_length += 1
            else:
                self._end_run_length(color, False)
            self._last_color = color

        self._end_run_length(0, True)

    def encode(self) -> ImageData:
        self._reset()
        granularity = 0
        if self._flags & ImageData.FLAG_RAW:
            # implicitly indexed raw image, reset state at the end of each row
            granularity = 1
        elif self._indexed and self.index_granularity.mode == IndexGranularityMode.MAX_SIZE:
            # increase row granularity until size spec is exceeded
            while True:
                self._encode_with_granularity(granularity + 1)
                if len(self.index.entries) == 1:
                    # index is empty (except for y=0), so don't index image.
                    granularity = self.region.height()
                    break
                if max(self.index.entries) > self.index_granularity.value:
                    break
                granularity += 1
            if granularity == 0:
                # cannot achieve specified index granularity (size too low)
                granularity = 1
        else:
            granularity = self.index_granularity.value

        self._encode_with_granularity(granularity)

        if len(self.index.entries) == 1 or self._flags & ImageData.FLAG_RAW:
            self._indexed = False
            self.index = None
        if self._indexed:
            self._flags |= ImageData.FLAG_INDEXED
            self.index.entries[0] = len(self.index.entries)
            if max(self.index.entries) > ImageIndex.MAX_ENTRY:
                raise EncodeError("cannot achieve specified index granularity (too many rows)")

        return ImageData(self._flags, self.alpha_color,
                         self.region.width(), self.region.height(),
                         self.index, self._data)

    def _get_max_run_length(self) -> int:
        """Get the maximum run length that can be encoded by this encoder.
        Can return 0 to indicate that the encoder doesn't support run lengths (raw encoder)"""
        raise NotImplementedError

    @classmethod
    def get_gray_levels(cls) -> int:
        """Get the number of gray levels encoded by this encoder."""
        raise NotImplementedError

    def _end_run_length(self, color: int, align_and_reset: bool) -> None:
        """End current run length if not zero. If `align_and_reset` is true, this method
        should end the current sequence, reset the encoder state, and align on byte boundary.
        If not, then this is called when a run length ends after color changes,
        or when the run length reaches the maximum length."""
        pass

    def _encode_with_granularity(self, granularity: int) -> None:
        """Encode image data using given index granularity (in number of rows),
        excluding header and index (only image data)."""
        self._reset()
        self.index = ImageIndex(granularity)
        self._iterate_pixels()


class ImageEncoderBinary(ImageEncoder, abc.ABC):
    def __init__(self, image: Image):
        super().__init__(image)
        self._flags |= ImageData.FLAG_BINARY

    @classmethod
    def get_gray_levels(cls) -> int:
        return 2


class ImageEncoderBinaryRaw(ImageEncoderBinary):
    """
    Image encoder for binary images (black or white only), raw encoding only.
    All data is pixel data, with least significant bit of each byte coming first.
    """
    _color_bits: int
    _color_byte: int

    def __init__(self, image: Image):
        super().__init__(image)
        self._flags |= ImageData.FLAG_RAW

    def _get_max_run_length(self) -> int:
        return 0

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0
        self._color_byte = 0

    def _end_run_length(self, color: int, align_and_reset: bool) -> None:
        data = self._data
        if align_and_reset:
            if self._color_bits != 0:
                data.append(self._color_byte)
                self._color_bits = 0
        else:
            self._color_byte |= color << self._color_bits
            self._color_bits += 1
            if self._color_bits == 8:
                data.append(self._color_byte)
                self._color_byte = 0
                self._color_bits = 0


class ImageEncoderBinaryMixed(ImageEncoderBinary):
    """
    Image encoder for binary images (black or white only)
    The encoding is a mix of run-length encoding and raw encoding.
    There are 2 defined token types:

    - Raw byte: MSB is not set. The remaining 7 bits is raw data for the next pixels.
                The data is encoded from MSB (bit 6) to LSB (bit 0).
    - RLE byte: MSB is set. Bit 6 indicates the run-length color (0=white, 1=black).
                The remaining 6 bits indicate the run length, minus 8.
                Thus the run length can be from 8 to 71.
    """
    _color_bits: int

    RLE_OFFSET = 8

    def _get_max_run_length(self) -> int:
        # - there are 6 bits to encode run length
        # - run length is encoded with offset of 8.
        # - if a raw byte is currently being encoded, it will have to be finished
        #   to encode run length. This number of bits can be added to max run length.
        return 63 + ImageEncoderBinaryMixed.RLE_OFFSET + \
               (0 if not self._color_bits else (7 - self._color_bits))

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0

    def _end_run_length(self, color: int, align_and_reset: bool) -> None:
        while self._run_length > 0 and (self._color_bits != 0 or self._run_length <= 7):
            # Raw encoding (either finish current byte then encode as RLE byte
            # or continue encoding as raw if run length is too small)
            if self._color_bits == 0:
                self._data.append(0x00)

            # add bits to existing raw byte
            encoded_length = min(self._run_length, 7 - self._color_bits)
            mask = 0
            for i in range(encoded_length):
                mask |= self._last_color << i
            self._data[-1] = (self._data[-1] << encoded_length) | mask

            self._color_bits += encoded_length
            self._run_length -= encoded_length
            if self._color_bits == 7:
                # end of raw byte
                self._color_bits = 0

        if self._run_length > 0:
            # RLE encoding (only better if run length is 8 or more)
            assert self._run_length >= 8
            self._data.append(0x80 | (self._last_color << 6) |
                              (self._run_length - ImageEncoderBinaryMixed.RLE_OFFSET))

        if align_and_reset:
            self._run_length = 0
            if self._color_bits != 0:
                # push last raw data bits so that first pixel is MSB
                self._data[-1] = (self._data[-1] << (7 - self._color_bits))
                self._color_bits = 0
            self._last_color = ImageEncoder._LAST_COLOR_NONE
        else:
            self._run_length = 1


class ImageEncoderGray(ImageEncoder, abc.ABC):
    @classmethod
    def get_gray_levels(cls) -> int:
        return 16


class ImageEncoderGrayRaw(ImageEncoderGray):
    """
    Image encoder for 4-bit gray images, raw encoding only.
    All data is pixel data, with least significant nibble coming first.
    Each nibble in a byte encodes data for 2 pixels.
    """
    _color_bits: int
    _color_byte: int

    def __init__(self, image: Image):
        super().__init__(image)
        self._flags |= ImageData.FLAG_RAW

    def _get_max_run_length(self) -> int:
        return 0

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0
        self._color_byte = 0

    def _end_run_length(self, color: int, align_and_reset: bool) -> None:
        data = self._data
        if align_and_reset:
            if self._color_bits != 0:
                data.append(self._color_byte)
                self._color_bits = 0
        elif self._color_bits == 0:
            self._color_byte = color
            self._color_bits = 4
        else:
            self._color_byte |= color << 4
            data.append(self._color_byte)
            self._color_bits = 0


class ImageEncoderGrayMixed(ImageEncoderGray):
    """
    Image encoder for 4-bit gray images.
    The encoding is a mix of run-length encoding and raw encoding.
    There are 3 defined token types:

    - Raw sequence: starts with a byte indicating the number of "raw" color bytes that follow.
        The MSB on the length byte is 0. The remaining bits indicated the length, minus one.
        Thus a raw sequence can have a length of 1 to 128.
    - RLE length: the MSB is 1, the remaining bits indicate the run length, minus 4.
        Thus the run length can be 3 to 130. The color for the run is defined by the
        second nibble if a RLE color byte has already been parsed, or the first nibble
        if a color byte has not been parsed.
    - RLE color: two nibbles indicating the color for the RLE immediately before and
        the next one encountered in the stream.

    The first byte can be either a raw sequence length byte or a RLE byte.
    The encoder state is reset on an index bound.
    """

    # current nibble in last raw sequence color byte
    _color_bits: int
    # index of the raw sequence length byte
    _raw_length_pos: int
    # index of the RLE color byte
    _rle_color_pos: int
    # minimum length required to use RLE
    _min_run_length: int

    RAW_OFFSET = 1
    RLE_OFFSET = 3

    def _get_max_run_length(self) -> int:
        return 127 + ImageEncoderGrayMixed.RLE_OFFSET

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0
        self._raw_length_pos = -1
        self._rle_color_pos = -1

    def _end_raw_sequence(self) -> None:
        if self._raw_length_pos != -1:
            self._data[self._raw_length_pos] = len(self._data) - self._raw_length_pos - \
                                               ImageEncoderGrayMixed.RAW_OFFSET - 1
            self._raw_length_pos = -1
            self._raw_color_nibble = 0

    def _end_run_length(self, color: int, align_and_reset: bool) -> None:
        data = self._data
        if self._run_length == 0:
            return

        if self._run_length >= self._min_run_length:
            if self._raw_length_pos != -1 and self._color_bits != 0:
                # complete byte in raw sequence.
                data[-1] = data[-1] | self._last_color << 4
                self._run_length -= 1
            self._end_raw_sequence()

            data.append(0x80 | (self._run_length - ImageEncoderGrayMixed.RLE_OFFSET))
            if self._rle_color_pos == -1:
                self._rle_color_pos = len(data)
                data.append(self._last_color)
            else:
                data[self._rle_color_pos] = data[self._rle_color_pos] | self._last_color << 4
                self._rle_color_pos = -1
        else:
            while self._run_length > 0:
                if self._raw_length_pos == -1:
                    self._raw_length_pos = len(data)
                    data.append(0x00)  # temporary value, length yet unknown.
                    self._color_bits = 0
                if self._color_bits == 0:
                    data.append(self._last_color)
                    self._color_bits = 4
                else:
                    data[-1] = data[-1] | self._last_color << 4
                    self._color_bits = 0
                    # -1 since raw_length_pos is the byte before the start of sequence,
                    # and another -1 since the sequence length has an offset of 1.
                    raw_seq_length = len(data) - self._raw_length_pos - \
                                     ImageEncoderGrayMixed.RAW_OFFSET - 1
                    if raw_seq_length == 127:
                        self._end_raw_sequence()
                self._run_length -= 1

        if align_and_reset:
            self._end_raw_sequence()
            self._run_length = 0
            self._rle_color_pos = -1
            self._last_color = ImageEncoder._LAST_COLOR_NONE
        else:
            self._run_length = 1

    def _encode_with_granularity(self, granularity: int) -> None:
        min_data = None
        min_index = None
        # try various min run lengths, one will give a smaller encoding.
        for i in range(4, 8):
            self._reset()
            self.index = ImageIndex(granularity)
            self._min_run_length = i
            self._iterate_pixels()
            if not min_data or len(self._data) < len(min_data):
                min_data = self._data
                min_index = self.index
        self._data = min_data
        self.index = min_index


@dataclass
class Config:
    input_file: Path
    output_file: str
    region: Rect
    indexed: bool
    index_granularity: IndexGranularity
    encoding: ImageEncoding
    opaque: bool
    verbose: bool


@dataclass(frozen=True)
class ImagePackResult(PackResult):
    image_data: ImageData

    def __repr__(self) -> str:
        d = self.image_data
        s = super().__repr__()
        s += f", {d.width}x{d.height} px,\n"
        s += ("raw" if d.flags & ImageData.FLAG_RAW else "mixed") + " "
        s += ("1-bit" if d.flags & ImageData.FLAG_BINARY else "4-bit")
        s += (" with alpha" if d.flags & ImageData.FLAG_ALPHA else "") + ", "
        if d.flags & ImageData.FLAG_INDEXED:
            s += f"indexed every {d.index.granularity} rows, " \
                 f"max {max(d.index.entries[1:])} bytes between entries"
        else:
            s += f"not indexed"
        return s


@dataclass(frozen=True)
class ImageObject(DataObject):
    file: Path
    region: Optional[Rect]
    indexed: bool
    index_granularity: str
    encoding: ImageEncoding
    opaque: bool

    def pack(self) -> ImagePackResult:
        try:
            index_granularity = IndexGranularity.from_str(self.index_granularity)
        except ValueError as e:
            raise PackError(f"invalid index granularity: {e}")
        config = Config(self.file, "", self.region, self.indexed,
                        index_granularity, self.encoding, self.opaque, False)

        try:
            image_data = create_image_data(config)
            data = image_data.encode()
        except EncodeError as e:
            raise PackError(f"encoding error: {e}")
        return ImagePackResult(data, image_data)

    def is_in_unified_data_space(self) -> bool:
        return True

    def get_type_name(self) -> str:
        return "image"


def register_builder(packer,
                     default_raw: bool = False,
                     default_indexed: bool = True,
                     default_index_gran: str =
                     repr(ImageEncoder.DEFAULT_INDEX_GRANULARITY)) -> None:
    @packer.file_builder
    def image(filename: Path, *,
              region: Optional[Tuple[int, int, int, int]] = None,
              raw: bool = default_raw,
              binary: Optional[bool] = None,
              opaque: bool = False,
              indexed: bool = default_indexed,
              index_granularity: str = default_index_gran) -> ImageObject:
        return ImageObject(filename, None if region is None else Rect(*region),
                           indexed, index_granularity, ImageEncoding(binary, raw), opaque)


def register_helper(packer) -> None:
    @packer.helper
    def set_image_defaults(raw: bool = False, indexed: bool = True,
                           index_gran: str = repr(ImageEncoder.DEFAULT_INDEX_GRANULARITY)) -> None:
        # register builder again with different defaults
        register_builder(packer, raw, indexed, index_gran)


def create_config(args: argparse.Namespace) -> Config:
    input_file = Path(args.input_file)
    if not input_file.exists() or not input_file.is_file():
        raise EncodeError("invalid input file")

    output_file = args.output_file

    region = None
    if args.region:
        parts = args.region.split(",")
        if len(parts) != 4:
            raise EncodeError("invalid region definition (expected 4 components)")
        try:
            values = [int(part) for part in parts]
        except ValueError:
            raise EncodeError("invalid region definition (bad component)")
        region = Rect(*values)

    index_granularity = ImageEncoder.DEFAULT_INDEX_GRANULARITY
    if not args.no_index:
        try:
            index_granularity = IndexGranularity.from_str(args.index_granularity)
        except ValueError as e:
            raise EncodeError(f"invalid index granularity: {e}")

    binary = None
    if args.force_1bit:
        binary = True
    if args.force_4bit:
        if args.force_1bit:
            raise EncodeError("cannot specify both --binary (-1) and --gray (-4)")
        binary = False

    raw = False
    if args.force_raw:
        raw = True
    if args.force_4bit:
        if args.force_1bit:
            raise EncodeError("cannot specify both --mixed (-M) and --raw (-R)")
        raw = False

    if raw and index_granularity != ImageEncoder.DEFAULT_INDEX_GRANULARITY:
        raise EncodeError("cannot specify index granularity for raw image")

    verbose = output_file != STD_IO

    return Config(input_file, output_file, region, not args.no_index,
                  index_granularity, ImageEncoding(binary, raw), args.force_opaque, verbose)


parser = argparse.ArgumentParser(description="Image encoding utility for game console")
parser.add_argument(
    "input_file", action="store", type=str,
    help="Input PNG file. Will be converted to 4-bit grayscale.")
parser.add_argument(
    "output_file", action="store", type=str, default=STD_IO, nargs="?",
    help=f"Output file. Can be set to '{STD_IO}' for standard output (default).")
parser.add_argument(
    "-r", "--region", action="store", type=str, dest="region",
    help="Bounds of the region to encode in the format <left>,<top>,<right>,<bottom>.")
parser.add_argument(
    "-g", "--index-granularity", action="store", type=str, dest="index_granularity",
    default=repr(ImageEncoder.DEFAULT_INDEX_GRANULARITY),
    help="Override index granularity with different value "
         "(can be ignored if it cannot be encoded). "
         "The granularity the number of rows between each index entry. "
         "Can be a number of rows (<n>r) or a maximum number of bytes (<n>b). "
         f"Default is '{ImageEncoder.DEFAULT_INDEX_GRANULARITY}'.")
parser.add_argument(
    "-I", "--no-index", action="store_true", dest="no_index",
    help="Do not include include index in encoded image. Reduces size but increases drawing time.")
parser.add_argument(
    "-1", "--binary", action="store_true", dest="force_1bit",
    help="Force 1-bit encoding (overrides default if the input image is not binary)")
parser.add_argument(
    "-4", "--gray", action="store_true", dest="force_4bit",
    help="Force 4-bit encoding (overrides default if the input image is binary)")
parser.add_argument(
    "-M", "--mixed", action="store_true", dest="force_mixed",
    help="Force mixed encoding (this has no effect since default is mixed)")
parser.add_argument(
    "-R", "--raw", action="store_true", dest="force_raw",
    help="Force raw encoding (overrides default mixed encoding)")
parser.add_argument(
    "-o", "--opaque", action="store_true", dest="force_opaque",
    help="Treat transparency as black if image has an alpha channel")


def create_image_data(config: Config) -> ImageData:
    image = Image.open(config.input_file)
    if not config.region:
        config.region = Rect(0, 0, image.width - 1, image.height - 1)
    elif config.region.width() <= 0 or config.region.height() <= 0:
        raise EncodeError("invalid region definition (negative size)")
    elif config.region.left >= image.width or config.region.right >= image.width or \
            config.region.top >= image.height or config.region.bottom >= image.height:
        raise EncodeError("region exceeds image dimensions")

    if image.mode != "LA":
        # convert image to grayscale + alpha if not already
        # this will also convert palette images to 8-bit gray
        image = image.convert("LA")

    # if color encoding was not forced, check all pixels in case image is binary
    # binary image encoding is preferable due to its smaller size.
    # transparent is treated as black during this check.
    # also list colors present in image to choose color to treat as alpha.
    levels = ImageEncoderGray.get_gray_levels()
    colors = set()
    has_alpha = False
    auto_bit_depth = (config.encoding.binary is None)
    if auto_bit_depth:
        config.encoding.binary = True
    for px in pixel_iterator(image, levels, config.region):
        if px == TRANSPARENT_COLOR:
            has_alpha = True
            continue
        if not config.opaque:
            colors.add(px)
        if auto_bit_depth and px != 0 and px != levels - 1:
            config.encoding.binary = False

    alpha_color = ImageEncoder.ALPHA_COLOR_NONE
    if has_alpha and not config.opaque and not config.encoding.binary:
        alpha_color = next((i for i in range(levels) if i not in colors), None)
        if alpha_color is None:
            raise EncodeError("cannot pick color for alpha, all colors are used")

    # choose encoder class for chosen encoding
    if config.encoding.binary:
        if config.encoding.raw:
            encoder = ImageEncoderBinaryRaw(image)
        else:
            encoder = ImageEncoderBinaryMixed(image)
    else:
        if config.encoding.raw:
            encoder = ImageEncoderGrayRaw(image)
        else:
            encoder = ImageEncoderGrayMixed(image)

    # encode image
    encoder.region = config.region
    encoder.index_granularity = config.index_granularity
    encoder.indexed = config.indexed
    encoder.alpha_color = alpha_color
    return encoder.encode()


def main():
    args = parser.parse_args()
    config = create_config(args)
    image_data = create_image_data(config)
    data = image_data.encode()

    # print information on encoded image
    if config.verbose:
        nbit = 1 if config.encoding.binary else 4
        raw_mixed = "raw" if image_data.flags & ImageData.FLAG_RAW else "mixed"
        alpha = " with alpha" if image_data.flags & ImageData.FLAG_ALPHA else ""
        print(f"Image is {config.region.width()} x {config.region.height()} px, "
              f"{raw_mixed} {nbit}-bit{alpha}, encoded in {len(data)} bytes")
        if image_data.flags & ImageData.FLAG_INDEXED:
            print(f"Indexed every {image_data.index.granularity} rows "
                  f"(max {max(image_data.index.entries[1:])} bytes between entries)")
        else:
            print("Not indexed.")

    # output encoded data
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
    except EncodeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        exit(1)
