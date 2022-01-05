#!/usr/bin/env python3

# Usage:
#
#     ./image_gen.py --help
#     ./image_gen.py <input_file> [output_file]
#
import abc
import argparse
import math
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import List, Optional, Generator, Callable

from PIL import Image

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


def pixel_iterator(image: Image, levels: int, rect: Optional[Rect] = None,
                   row_cb: Optional[Callable[[int], None]] = None) -> Generator[int, None, None]:
    if not rect:
        rect = Rect(0, 0, image.width - 1, image.height - 1)
    for y in range(rect.top, rect.bottom + 1):
        if row_cb:
            row_cb(y)
        for x in range(rect.left, rect.right + 1):
            pixel = image.getpixel((x, y))
            yield math.floor((pixel / 256) * levels)


class ImageIndex:
    granularity: int
    entries: List[int]

    MAX_ENTRY = 256

    def __init__(self, granularity: int):
        self.granularity = granularity
        self.entries = []

    def encode(self) -> bytes:
        data = bytearray([self.granularity])
        for entry in self.entries:
            if not (0 < entry <= ImageIndex.MAX_ENTRY):
                raise ValueError("Cannot encode index, out of bounds")
            data.append(entry - 1)
        return data


class ImageEncoder(abc.ABC):
    image: Image
    region: Rect
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

    HEADER_SIZE = 3

    FLAG_BINARY = 1
    FLAG_INDEXED = 2

    DEFAULT_INDEX_GRANULARITY = IndexGranularity(IndexGranularityMode.MAX_SIZE, 64)

    _LAST_COLOR_NONE = -1

    def __init__(self, image: Image):
        self._flags = 0
        self.image = image
        self.region = Rect(0, 0, image.width - 1, image.height - 1)
        self.indexed = True
        self.index_granularity = ImageEncoder.DEFAULT_INDEX_GRANULARITY
        self._reset()

    def _reset(self) -> None:
        self.index = None
        self._indexed = self.indexed
        self._data = bytearray()
        self._last_data_len = 0
        self._run_length = 0
        self._last_color = ImageEncoder._LAST_COLOR_NONE
        self._create_header()

    def _create_header(self) -> bytes:
        header = bytearray()
        if self._indexed:
            self._flags |= ImageEncoder.FLAG_INDEXED
        else:
            self._flags &= ~ImageEncoder.FLAG_INDEXED
        header.append(self._flags)
        header.append(self.region.width() - 1)
        header.append(self.region.height() - 1)
        return header

    def _iterate_pixels(self) -> None:
        def row_cb(y: int) -> None:
            if y % self.index.granularity == 0 and self._indexed:
                self._end_run_length(True)
                self.index.entries.append(len(self._data) - self._last_data_len)
                self._last_data_len = len(self._data)

        for color in pixel_iterator(self.image, self.get_gray_levels(), self.region, row_cb):
            if (self._last_color == ImageEncoder._LAST_COLOR_NONE or color == self._last_color) \
                    and self._run_length < self._get_max_run_length():
                self._run_length += 1
            else:
                self._end_run_length(False)
            self._last_color = color

        self._end_run_length(True)

    def encode(self) -> bytes:
        self._reset()
        granularity = 0
        if self._indexed and self.index_granularity.mode == IndexGranularityMode.MAX_SIZE:
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
                print("Cannot achieve specified index granularity (size too low)")
                granularity = 1
        else:
            granularity = self.index_granularity.value

        self._encode_with_granularity(granularity)

        # insert header
        if len(self.index.entries) == 1:
            self._indexed = False
        self._data[0:0] = self._create_header()

        # insert index if needed
        if self._indexed:
            if max(self.index.entries) > ImageIndex.MAX_ENTRY:
                raise EncodeError("cannot achieve specified index granularity (too many rows)")

            idx = ImageEncoder.HEADER_SIZE
            self.index.entries[0] = len(self.index.entries)  # first entry is length of index
            self._data[idx:idx] = self.index.encode()

        return self._data

    def _get_max_run_length(self) -> int:
        """Get the maximum run length that can be encoded by this encoder."""
        raise NotImplementedError

    def get_gray_levels(self) -> int:
        """Get the number of gray levels encoded by this encoder."""
        raise NotImplementedError

    def _end_run_length(self, align_and_reset: bool) -> None:
        """End current run length if not zero. If `align_and_reset` is true, this method
        should end the current sequence, reset the encoder state, and align on byte boundary.
        If not, then this is called when a run length ends after color changes,
        or when the run length reaches the maximum length."""
        raise NotImplementedError

    def _encode_with_granularity(self, granularity: int) -> None:
        """Encode image data using given index granularity (in number of rows),
        excluding header and index (only image data)."""
        raise NotImplementedError


class ImageEncoderBinary(ImageEncoder):
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

    def __init__(self, image: Image):
        super().__init__(image)
        self._flags |= ImageEncoder.FLAG_BINARY

    def _get_max_run_length(self) -> int:
        # - there are 6 bits to encode run length
        # - run length is encoded with offset of 8.
        # - if a raw byte is currently being encoded, it will have to be finished
        #   to encode run length. This number of bits can be added to max run length.
        return 63 + ImageEncoderBinary.RLE_OFFSET + \
               (0 if not self._color_bits else (7 - self._color_bits))

    def get_gray_levels(self) -> int:
        return 2

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0

    def _end_run_length(self, align_and_reset: bool) -> None:
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
                              (self._run_length - ImageEncoderBinary.RLE_OFFSET))

        if align_and_reset:
            self._run_length = 0
            if self._color_bits != 0:
                # push last raw data bits so that first pixel is MSB
                self._data[-1] = (self._data[-1] << (7 - self._color_bits))
                self._color_bits = 0
            self._last_color = ImageEncoder._LAST_COLOR_NONE
        else:
            self._run_length = 1

    def _encode_with_granularity(self, granularity: int) -> None:
        self._reset()
        self.index = ImageIndex(granularity)
        self._iterate_pixels()


class ImageEncoderGray(ImageEncoder):
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

    def __init__(self, image: Image):
        super().__init__(image)

    def _get_max_run_length(self) -> int:
        return 127 + ImageEncoderGray.RLE_OFFSET

    def get_gray_levels(self) -> int:
        return 16

    def _reset(self) -> None:
        super()._reset()
        self._color_bits = 0
        self._raw_length_pos = -1
        self._rle_color_pos = -1

    def _end_raw_sequence(self) -> None:
        if self._raw_length_pos != -1:
            self._data[self._raw_length_pos] = len(self._data) - self._raw_length_pos - \
                                               ImageEncoderGray.RAW_OFFSET - 1
            self._raw_length_pos = -1
            self._raw_color_nibble = 0

    def _end_run_length(self, align_and_reset: bool) -> None:
        data = self._data
        if self._run_length == 0:
            return

        if self._run_length >= self._min_run_length:
            if self._raw_length_pos != -1 and self._color_bits != 0:
                # complete byte in raw sequence.
                data[-1] = data[-1] | self._last_color << 4
                self._run_length -= 1
            self._end_raw_sequence()

            data.append(0x80 | (self._run_length - ImageEncoderGray.RLE_OFFSET))
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
                                     ImageEncoderGray.RAW_OFFSET - 1
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
    binary: bool
    verbose: bool


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
        if region.width() <= 0 or region.height() <= 0:
            raise EncodeError("invalid region definition (negative size)")

    index_granularity = ImageEncoder.DEFAULT_INDEX_GRANULARITY
    if not args.no_index:
        ig_def = args.index_granularity
        mode: IndexGranularityMode
        if ig_def[-1] == 'b':
            mode = IndexGranularityMode.MAX_SIZE
        elif ig_def[-1] == 'r':
            mode = IndexGranularityMode.ROW_COUNT
        else:
            raise EncodeError("invalid index granularity mode")
        try:
            value = int(ig_def[:-1])
        except ValueError:
            raise EncodeError("invalid index granularity value")
        if mode == IndexGranularityMode.MAX_SIZE and \
                not (0 < value <= ImageIndex.MAX_ENTRY) or \
                mode == IndexGranularityMode.ROW_COUNT and \
                not (0 < value < ImageEncoder.MAX_IMAGE_SIZE):
            raise EncodeError("index granularity value out of bounds")

        index_granularity = IndexGranularity(mode, value)

    verbose = output_file != STD_IO

    return Config(input_file, output_file, region, not args.no_index,
                  index_granularity, args.binary, verbose)


parser = argparse.ArgumentParser(description="Image encoding utility for game console")
parser.add_argument(
    "input_file", action="store", type=str,
    help="Input PNG file. Will be converted to 4-bit grayscale.")
parser.add_argument(
    "output_file", action="store", type=str, default=STD_IO, nargs="?",
    help="Output file. Can be set to '-' for standard output (default).")
parser.add_argument(
    "-r", "--region", action="store", type=str, dest="region",
    help="Bounds of the region to encode in the format <left>,<top>,<right>,<bottom>.")
parser.add_argument(
    "-g", "--index-granularity", action="store", type=str, dest="index_granularity",
    default="64b",
    help="Override index granularity with different value "
         "(can be ignored if it cannot be encoded). "
         "The granularity the number of rows between each index entry. "
         "Can be a number of rows (<n>r) or a maximum number of bytes (<n>b). "
         "Default is '64b'.")
parser.add_argument(
    "-I", "--no-index", action="store_true", dest="no_index",
    help="Do not include include index in encoded image. Reduces size but increases drawing time.")
parser.add_argument(
    "-b", "--binary", action="store_true", dest="binary",
    help="Force 1-bit encoding (can be used if the input image is not binary)")


def main():
    args = parser.parse_args()
    config = create_config(args)

    image = Image.open(config.input_file)
    if not config.region:
        config.region = Rect(0, 0, image.width - 1, image.height - 1)
    elif config.region.left >= image.width or config.region.right >= image.width or \
            config.region.top >= image.height or config.region.bottom >= image.height:
        raise EncodeError("Region exceeds image dimensions")

    if image.mode != "L":
        # convert image to grayscale if not already
        # this will also convert palette images to 8-bit gray
        image = image.convert("L")

    # if binary was not forced, check all pixels in case image is binary
    # binary images encoding is much terser.
    if not config.binary:
        config.binary = True
        levels = ImageEncoderGray(image).get_gray_levels()
        for px in pixel_iterator(image, levels, config.region):
            if px != 0 and px != levels - 1:
                config.binary = False
                break

    # encode image
    if config.binary:
        encoder = ImageEncoderBinary(image)
    else:
        encoder = ImageEncoderGray(image)
    encoder.region = config.region
    encoder.index_granularity = config.index_granularity
    encoder.indexed = config.indexed
    data = encoder.encode()

    # print information on encoded image
    if config.verbose:
        nbit = 1 if config.binary else 4
        print(f"Image is {config.region.width()} x {config.region.height()} px, "
              f"{nbit}-bit, encoded in {len(data)} bytes")
        if encoder._indexed:
            print(f"Indexed every {encoder.index.granularity} rows "
                  f"(max {max(encoder.index.entries)} bytes between entries)")
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
