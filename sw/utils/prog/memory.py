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

import abc
import argparse
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Union

from bitarray import bitarray

from prog.comm import ProgError
from utils import parse_dec_or_hex_number, ProgressCallback, PathLike, print_progress_bar, \
    process_bitarray_sequences

STD_IO = "-"


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

    def get_size(self) -> int:
        """Returns the size of the memory device in bytes."""
        raise NotImplementedError

    def get_block_count(self) -> int:
        """Returns the number of blocks on the device.
        A block is the minimum unit that can be erased."""
        return self.get_size() // self.get_smallest_erase_size()

    def requires_block_erase(self) -> bool:
        """To be overriden by subclass to return whether device requires block erase or not."""
        raise NotImplementedError

    def get_smallest_erase_size(self) -> int:
        """If block erase is required, this should return the smallest erase block size
        (must be a power of two)."""
        if self.requires_block_erase():
            raise NotImplementedError
        return 1

    def reset(self) -> None:
        """Reset memory device."""
        pass

    def read(self, address: int, count: int, progress: ProgressCallback) -> bytes:
        """Read from memory device, from start `address`, `count` bytes, with `progress` callback.
        Returns the read bytes."""
        raise NotImplementedError

    def write(self, address: int, data: bytes, progress: ProgressCallback) -> None:
        """Write `data` to memory device at start `address`, with `progress` callback."""
        raise NotImplementedError

    def erase_blocks(self, blocks: bitarray, progress: ProgressCallback) -> None:
        """Erase blocks on memory device before writing sequences, with `progress` callback.
        The `blocks` bit array indicates which blocks (the smallest block size) to erase."""
        if self.requires_block_erase():
            raise NotImplementedError

    def erase(self, progress: ProgressCallback) -> None:
        """Erase whole memory device."""
        raise NotImplementedError


class MemoryLocal(MemoryDriver):
    """Memory driver for local memory stored in a file."""
    filename: Path
    erase_byte: int
    erase_before_write: bool
    block_erase_size: int

    data: bytearray

    def __init__(self, filename: PathLike, size: int, erase_byte: int,
                 block_erase_size: int, erase_before_write: bool):
        """Initialize local memory by reading a file. If the file doesn't exist,
        the memory is initialized to the erase byte and with a size in bytes."""
        self.filename = Path(filename)
        self.erase_byte = erase_byte
        self.block_erase_size = block_erase_size
        self.erase_before_write = erase_before_write

        self.data = bytearray(size)
        for i in range(size):
            self.data[i] = erase_byte
        if self.filename.exists():
            try:
                with open(self.filename, "rb") as file:
                    data = file.read()
                    self.data[:min(size, len(data))] = data
            except IOError as e:
                raise ProgError(f"failed to read file '{filename}': {e}")

    def get_size(self) -> int:
        return len(self.data)

    def requires_block_erase(self) -> bool:
        return self.erase_before_write

    def get_smallest_erase_size(self) -> int:
        return self.block_erase_size

    def read(self, address: int, count: int, progress: ProgressCallback) -> bytes:
        progress(count, count)
        return self.data[address:address + count]

    def write(self, address: int, data: bytes, progress: ProgressCallback) -> None:
        self.data[address:address + len(data)] = data
        progress(len(data), len(data))

    def erase_blocks(self, blocks: bitarray, progress: ProgressCallback) -> None:
        pos = 0
        block_size = self.get_smallest_erase_size()
        for i in range(self.get_block_count()):
            if blocks[i]:
                for j in range(block_size):
                    self.data[pos + j] = self.erase_byte
            pos += block_size
        total_erased = blocks.count() * block_size
        progress(total_erased, total_erased)

    def erase(self, progress: ProgressCallback) -> None:
        all_blocks = bitarray(self.get_block_count())
        all_blocks.setall(True)
        self.erase_blocks(all_blocks, progress)

    def save(self) -> None:
        """Save local memory to file."""
        try:
            with open(self.filename, "wb") as file:
                file.write(self.data)
        except IOError as e:
            raise ProgError(f"failed to write file '{self.filename}': {e}")


class MemoryManager:
    """Class used to batch multiple reads, writes and copies in a single operation to minimize
    the number of operations done on device. All operations are done on the initial state of the
    memory before the batch, so the order of the operations have no impact."""
    driver: MemoryDriver
    verbose: bool

    @dataclass(frozen=True)
    class Read:
        address: int
        size: int

    @dataclass(frozen=True)
    class Write:
        address: int
        data: bytes

    @dataclass(frozen=True)
    class Copy:
        from_addr: int
        to_addr: int
        size: int

    operations: List[Union[Read, Write, Copy]]

    def __init__(self, driver: MemoryDriver, verbose: bool = True):
        self.driver = driver
        self.verbose = verbose
        self.clear()

    def _check_addr(self, addr: int, size: int) -> int:
        return addr < 0 or addr + size > self.driver.get_size()

    def read(self, address: int, size: int) -> None:
        if self._check_addr(address, size):
            raise ValueError("read out of bounds")
        self.operations.append(MemoryManager.Read(address, size))

    def write(self, address: int, data: bytes) -> None:
        if self._check_addr(address, len(data)):
            raise ValueError("write out of bounds")
        self.operations.append(MemoryManager.Write(address, data))

    def copy(self, from_addr: int, to_addr: int, size: int) -> None:
        if self._check_addr(from_addr, size) and self._check_addr(to_addr, size):
            raise ValueError("copy out of bounds")
        self.operations.append(MemoryManager.Copy(from_addr, to_addr, size))

    def clear(self) -> None:
        self.operations = []

    def _mark_block(self, mask: bitarray, address: int, size: int) -> None:
        boundary = self.driver.get_smallest_erase_size()
        if size == 0 and boundary == 1:
            return
        start_block = address // boundary
        end_block = (address + size + (boundary - 1)) // boundary
        end_block = max(end_block, start_block + 1)  # special case if size == 0
        for i in range(start_block, end_block):
            mask[i] = True

    def execute(self) -> List[bytes]:
        if not self.operations:
            return []

        self.driver.reset()
        mem_size = self.driver.get_size()
        block_size = self.driver.get_smallest_erase_size()
        block_count = self.driver.get_block_count()

        # mark all blocks that must be read and written
        read_mask = bitarray(block_count)
        read_mask.setall(False)
        extend_read_mask = bitarray(block_count)
        extend_read_mask.setall(False)
        write_mask = bitarray(block_count)
        write_mask.setall(False)
        for op in self.operations:
            # writing requires erasing whole blocks therefore the start and end blocks
            # of write operations must be read first in case write is unaligned.
            if isinstance(op, MemoryManager.Read):
                self._mark_block(read_mask, op.address, op.size)
            elif isinstance(op, MemoryManager.Write):
                self._mark_block(extend_read_mask, op.address, 0)
                self._mark_block(extend_read_mask, op.address + len(op.data), 0)
                self._mark_block(write_mask, op.address, len(op.data))
            elif isinstance(op, MemoryManager.Copy):
                self._mark_block(read_mask, op.from_addr, op.size)
                self._mark_block(extend_read_mask, op.to_addr, 0)
                self._mark_block(extend_read_mask, op.to_addr + op.size, 0)
                self._mark_block(write_mask, op.to_addr, op.size)
        # to allow diffing, all written blocks will be also read beforehand
        read_mask |= extend_read_mask
        read_mask |= write_mask

        # read marked blocks from device
        def read_process(start: int, end: int, progress: ProgressCallback) -> None:
            read_data[start:end] = self.driver.read(start, end - start, progress)

        read_data = bytearray(mem_size)
        process_bitarray_sequences(self.driver, read_mask, "Reading", read_process, self.verbose)

        # extract all sequences corresponding to read operations
        read_sequences = []
        for op in self.operations:
            if isinstance(op, MemoryManager.Read):
                read_sequences.append(read_data[op.address:op.address + op.size])

        # create array containing all data to write
        write_data = bytearray(mem_size)
        # 1. write the starting and ending block of all writes in case they're unaligned
        pos = 0
        for i in range(block_count):
            end = pos + block_size
            if extend_read_mask[i]:
                write_data[pos:end] = read_data[pos:end]
            pos = end
        # 2. write the actual data to be written
        for op in self.operations:
            if isinstance(op, MemoryManager.Write):
                write_data[op.address:op.address + len(op.data)] = op.data
            elif isinstance(op, MemoryManager.Copy):
                write_data[op.to_addr:op.to_addr + op.size] = \
                    read_data[op.from_addr:op.from_addr + op.size]

        if all(isinstance(op, MemoryManager.Read) for op in self.operations):
            return read_sequences

        # compare data to write with existing data to avoid writing unnecessarily
        # unset all blocks in write mask that are identical
        pos = 0
        for i in range(block_count):
            end = pos + block_size
            if write_mask[i] and write_data[pos:end] == read_data[pos:end]:
                write_mask[i] = False
            pos = end

        # at this point check if there's actually anything to write
        if not write_mask.any():
            if self.verbose:
                print("No bytes to write, data is identical on device.")
            return read_sequences

        # erase all blocks to be written if needed
        if self.driver.requires_block_erase():
            self.driver.erase_blocks(write_mask,
                                     print_progress_bar("Erasing", verbose=self.verbose))

        # write marked blocks to device
        def write_process(start: int, end: int, progress: ProgressCallback) -> None:
            self.driver.write(start, write_data[start:end], progress)

        process_bitarray_sequences(self.driver, write_mask, "Writing", write_process, self.verbose)

        # verify written data against expected data
        def verify_process(start: int, end: int, progress: ProgressCallback) -> None:
            data = self.driver.read(start, end - start, progress)
            if data != write_data[start:end]:
                raise ProgError("verification failed, check serial connection and try again")

        process_bitarray_sequences(self.driver, write_mask, "Verifying",
                                   verify_process, self.verbose)

        self.operations.clear()


def create_parser(subparsers, size: int, name: str) -> argparse.ArgumentParser:
    """Add parser with a `name` to `subparsers` for a memory device with a certain `size`."""
    parser = subparsers.add_parser(
        name.lower(), help=f"Raw read and write access to {name}.",
        description=f"Allows raw read and write access to {name}. This command should be used"
                    "carefully as to not damage the index kept by other commands."
                    "This command performs no user confirmation before doing an operation.")
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
    return parser


def create_config(mem_size: int, args: argparse.Namespace) -> Config:
    """Parse and validate input arguments, return typed configuration object."""
    if args.erase and args.write:
        raise ProgError("cannot write and erase at the same time")

    try:
        address = parse_dec_or_hex_number(args.address)
    except ValueError:
        raise ProgError("invalid start address")
    if address < 0 or address >= mem_size:
        raise ProgError("start address out of bounds")

    try:
        size = parse_dec_or_hex_number(args.size)
    except ValueError:
        raise ProgError("invalid size")
    if size <= 0 or size > mem_size:
        raise ProgError("size out of bounds")

    if args.write:
        if args.input == STD_IO:
            input_data = sys.stdin.buffer.read(size)
        else:
            path = Path(args.input)
            if not path.exists():
                raise ProgError("input file does not exist")
            if not path.is_file():
                raise ProgError("input is not a file")
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

    writer = MemoryManager(driver, config.verbose)
    if config.read:
        writer.read(config.address, config.size)
    if config.write:
        writer.write(config.address, config.input_data)

    read_data = writer.execute()
    if config.read:
        try:
            with open(config.output_file, "wb") as file:
                file.write(read_data[0])
        except IOError as e:
            raise ProgError(f"could not write to file: {e}")

    if config.erase:
        # erase whole device
        driver.erase(print_progress_bar("Erasing", verbose=config.verbose))
