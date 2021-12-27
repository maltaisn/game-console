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

# Generic command line interface for reading & writing to memory chip on game console:
# - Supports unaligned page access.
# - Data written is diffed with existing data to save cycles and time.
# - All written data is verified.

import abc
import argparse
import math
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, List, Tuple

from comm import CommError
from utils import parse_dec_or_hex_number, ProgressCallback, readable_size

STD_IO = "-"

PROGRESS_BAR_WIDTH = 30
PROGRESS_BAR_MARGIN = 9

DiffSequences = List[Tuple[int, int]]


@dataclass
class Config:
    read: bool
    write: bool
    erase: bool
    address: int
    size: int
    input_data: bytes
    output_file: str
    verbose: bool


class MemoryDriver(abc.ABC):
    """Base class for a memory chip driver than can read, write & erase device."""

    def reset(self) -> None:
        """Reset memory device."""
        pass

    def read(self, address: int, count: int, progress: ProgressCallback) -> bytes:
        """Read from memory device, from start `address`, `count` bytes, with `progress` callback.
        Returns the read bytes."""
        raise NotImplementedError

    def write(self, address: int, data: bytes, count: int, sequences: DiffSequences,
              progress: ProgressCallback) -> None:
        """Write data to memory device at start `address`, with `progress` callback.
        Data is provided as a list of (start, end) sequences to write from `data`.
        For each sequence, to get actual start address on device, add `address`."""
        raise NotImplementedError

    def requires_block_erase(self) -> bool:
        """To be overriden by subclass to return whether device requires block erase or not."""
        raise NotImplementedError

    def get_smallest_erase_size(self) -> int:
        """If block erase is required, this should return the smallest erase block size
        (must be a power of two)."""
        if self.requires_block_erase():
            raise NotImplementedError
        return 1

    def erase_blocks(self, address: int, count: int, sequences: DiffSequences,
                     progress: ProgressCallback) -> None:
        """Erase blocks on memory device before writing sequences, with `progress` callback.
        The total number of bytes to erase is `count` and sequences start at `address`."""
        if self.requires_block_erase():
            raise NotImplementedError

    def erase(self, progress: ProgressCallback) -> None:
        """Erase whole memory device."""
        raise NotImplementedError

    def get_diff_sequences(self, data: bytes,
                           old_data: Optional[bytes]) -> Tuple[int, DiffSequences]:
        """Get a list of different sequences between data and old_data.
        Also returns the sum of all the sequences lengths. All sequences start and end on
        the smallest erase block boundary size."""
        boundary = self.get_smallest_erase_size()
        boundary_mask = ~(boundary - 1)
        start = -1
        end = -1
        total = 0
        sequences: DiffSequences = []
        for i in range(len(data) + 1):
            if i < end:
                # previous sequence was rounded up to boundary, skip those bytes
                continue
            last = i == len(data)
            if start != -1 and (last or data[i] == old_data[i]):
                # end of diff sequence, write it.
                # start and end should be on block boundary
                start = start & boundary_mask
                end = i
                if end & ~boundary_mask:
                    end = (end & boundary_mask) + boundary
                if sequences and sequences[-1][1] == start:
                    # start of this sequence is at end of last sequence, merge both
                    sequences[-1] = sequences[-1][0], end
                else:
                    sequences.append((start, end))
                total += end - start
                start = -1
            elif not last and start == -1 and data[i] != old_data[i]:
                start = i
        return total, sequences


def print_progress_bar(verbose: bool, name: str) -> ProgressCallback:
    """Print a progress bar of a certain width with a name padded to a
    number of columns (left_space). Returns a callback to use to update progress bar."""
    if not verbose:
        return lambda c, t: None

    start = time.time()

    def callback(current: int, total: int) -> None:
        progress = 1 if total == 0 else (current / total)
        n = math.floor(progress * PROGRESS_BAR_WIDTH)
        print("\033[2K", end="")  # erase previous bar
        print(f"{name.ljust(PROGRESS_BAR_MARGIN, ' ')} "  # title
              f"| {('#' * n)}{' ' * (PROGRESS_BAR_WIDTH - n)} | "  # progress bar
              f"{f'{math.floor(progress * 100):.0f}%,':<5} {f'{time.time() - start:.1f} s,':<8} "  # percent & time
              f"{readable_size(current)} / {readable_size(total)}\r", end="")  # curr & total size
        if current == total:
            print()

    return callback


def create_parser(subparsers, size: int, name: str) -> None:
    """Add parser with a `name` to `subparsers` for a memory device with a certain `size`."""
    parser = subparsers.add_parser(name.lower(), help=f"Read & write to {name}")
    parser.add_argument(
        "address", action="store", type=str, default="0", nargs="?",
        help="Start address for operation, can be decimal literal "
             "or hexadecimal literal (0x prefix)")
    parser.add_argument(
        "-r", "--read", action="store_true", default=False, dest="read",
        help="Enable read operation")
    parser.add_argument(
        "-w", "--write", action="store_true", default=False, dest="write",
        help="Enable write operation")
    parser.add_argument(
        "-e", "--erase", action="store_true", default=False, dest="erase",
        help="Erase whole device")
    parser.add_argument(
        "-s", "--size", action="store", type=str, default=str(size), dest="size",
        help="Read size in bytes, can be decimal or hexadecimal literal (0x prefix). "
             "By default, in read mode, the whole device is read. "
             "By default, in write mode, writing ends at device capacity or input size. "
             "This option has no effect on erase.")
    parser.add_argument(
        "-i", "--input", action="store", type=str, default=STD_IO, dest="input",
        help=f"Input file. Default is '{STD_IO}' for standard input.")
    parser.add_argument(
        "-o", "--output", action="store", type=str, default=STD_IO, dest="output",
        help=f"Output file. Default is '{STD_IO}' for standard output.")


def create_config(mem_size: int, args: argparse.Namespace) -> Config:
    """Parse and validate input arguments, return typed configuration object."""
    if args.erase and args.write:
        raise CommError("cannot write and erase at the same time")

    try:
        address = parse_dec_or_hex_number(args.address)
    except ValueError:
        raise CommError("invalid start address")
    if address < 0 or address >= mem_size:
        raise CommError("start address out of bounds")

    try:
        size = parse_dec_or_hex_number(args.size)
    except ValueError:
        raise CommError("invalid size")
    if size <= 0 or size > mem_size:
        raise CommError("size out of bounds")

    if args.write:
        if args.input == STD_IO:
            input_data = sys.stdin.buffer.read(size)
        else:
            path = Path(args.input)
            if not path.exists():
                raise CommError("input file does not exist")
            if not path.is_file():
                raise CommError("input is not a file")
            with open(path, "rb") as file:
                input_data = file.read(size)
        size = min(size, len(input_data))
    else:
        input_data = bytes()

    # prevent wrapping by limiting size to not pass the end of memory
    size = min(size, mem_size - address)

    # size may have been modified, update input data accordingly
    if size < len(input_data):
        input_data = input_data[:size]

    # if writing read data to std out, don't write status messages
    verbose = not args.read or args.output != STD_IO

    return Config(args.read, args.write, args.erase, address, size,
                  input_data, args.output, verbose)


def execute_config(driver: MemoryDriver, config: Config) -> None:
    """Execute previously created configuration to operate on memory device."""
    if not config.read and not config.write and not config.erase:
        print("No operations to do!")
        return

    driver.reset()

    # if writing, we need to read first to diff & if erasing partial pages
    # if only erasing, skip initial read
    if config.write or config.read:
        # writing requires reading whole blocks before they're erased.
        # adjust address and size to account for this
        boundary = driver.get_smallest_erase_size()
        extend_start = 0
        extend_end = 0
        if config.write:
            boundary_mask = ~(boundary - 1)
            start = config.address & boundary_mask
            extend_start = config.address - start
            end = config.address + config.size
            if end & ~boundary_mask:
                end = (end & boundary_mask) + boundary
                extend_end = end - config.address - config.size
            config.address = start
            config.size = end - start

        # read first, in any case
        read_data = driver.read(config.address, config.size,
                                print_progress_bar(config.verbose, "Reading"))

        if config.read:
            # read operation is enabled, output read data.
            output_data = read_data
            if config.write and (extend_start or extend_end):
                output_data = read_data[extend_start:-extend_end]
            if config.output_file == STD_IO:
                sys.stdout.buffer.write(output_data)
            else:
                with open(config.output_file, "wb") as file:
                    file.write(output_data)

    if config.write:
        # extend write data to include parts that will be erased beforehand
        if extend_start:
            config.input_data = read_data[:extend_start] + config.input_data
        if extend_end:
            config.input_data += read_data[-extend_end:]

        # diff existing data with new data
        total, sequences = driver.get_diff_sequences(config.input_data, read_data)

        if total:
            # if memory device requires erase before write, erase blocks for identified sequences.
            if driver.requires_block_erase():
                driver.erase_blocks(config.address, total, sequences,
                                    print_progress_bar(config.verbose, "Erasing"))

            # write new data to device
            driver.write(config.address, config.input_data, total, sequences,
                         print_progress_bar(config.verbose, "Writing"))

            # reread the written range and validate data
            # if verification fails just abort operation
            read_data = driver.read(config.address, len(config.input_data),
                                    print_progress_bar(config.verbose, "Verifying"))
            if read_data != config.input_data:
                raise CommError("verification failed")
        else:
            print("No bytes to write, data is identical on device")

    elif config.erase:
        # erase whole device
        driver.erase(print_progress_bar(config.verbose, "Erasing"))
