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
import argparse
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple, List

from comm import CommError
from spi import SpiInterface, SpiPeripheral
from utils import ProgressCallback, print_progress_bar

# This module is used to read, write & erase the EEPROM chip on the game console
# The EEPROM chip model number is FT25C32A
# Datasheet: https://www.fremontmicro.com/downfile.aspx?filepath=/upload/2019/0805/1604ntvhj2.pdf&filename=ft25c32a_ds_rev0.82.pdf

EEPROM_SIZE = 4096
# eeprom page size in bytes
PAGE_SIZE = 32

ERASE_BYTE = 0xff

# instruction set
INSTR_WREN = 0x6
INSTR_WRDI = 0x4
INSTR_RDSR = 0x5
INSTR_WRSR = 0x1
INSTR_READ = 0x3
INSTR_WRITE = 0x2

STD_IO = "-"


class EepromDriver:
    """Class used to read & write to the EEPROM chip."""
    spi: SpiInterface

    def __init__(self, spi: SpiInterface):
        self.spi = spi

    def read(self, start: int, count: int,
             progress: Optional[ProgressCallback]) -> bytes:
        """Read a number of bytes from EEPROM from a start address.
        A callback can be used to show progress. Returns the read bytes."""
        if count == 0:
            return bytes()

        self._wait_ready()
        command = [INSTR_READ, (start >> 8) & 0xff, start & 0xff]
        # insert dummy MOSI bytes
        for i in range(count):
            command.append(0)

        received = self.spi.transceive(SpiPeripheral.EEPROM, command,
                                       progress=None if progress is None else
                                       lambda c, t: progress(c - 3, t - 3))
        return received[3:]

    def _write(self, address: int, data: bytes,
               progress: Optional[ProgressCallback]) -> None:
        """Write bytes to EEPROM at an address."""
        # wait until ready and enable write latch
        written = 0
        it = iter(data)
        while written < len(data):
            self._wait_ready()
            self.spi.transceive(SpiPeripheral.EEPROM, [INSTR_WREN])
            # write data for one page
            command = [INSTR_WRITE, (address >> 8) & 0xff, address & 0xff]
            page_end = (address & ~(PAGE_SIZE - 1)) + PAGE_SIZE
            while written < len(data) and address < page_end:
                byte = next(it, None)
                if byte is None:
                    break
                command.append(byte)
                written += 1
                address += 1
            self.spi.transceive(SpiPeripheral.EEPROM, command)
            if progress:
                progress(written, len(data))

    def write(self, address: int, count: int, data: bytes, old_data: bytes,
              progress: Optional[ProgressCallback]) -> int:
        """Write a number of bytes to EEPROM at an address. Returns the number of bytes
        written. A callback can be used to show progress. Old data is provided to make a diff
        and only update the minimum to save write cycles and improve speed."""
        # identify all sequences to write (diffing)
        start = -1
        total = 0
        sequences: List[Tuple[int, int]] = []
        for i in range(count + 1):
            if start != -1 and (i == count or data[i] == old_data[i]):
                # end of diff sequence, write it.
                sequences.append((start, i))
                total += i - start
                start = -1
            elif i != count and start == -1 and data[i] != old_data[i]:
                start = i

        # write new data in identified sequences
        if sequences:
            written = 0
            progress_delegate = None if progress is None else \
                lambda c, t: progress(written + c, total)
            for start, end in sequences:
                self._write(start + address, data[start:end], progress_delegate)
                written += end - start
        elif progress:
            # no data to write, content identical
            progress(0, 0)

        return total

    def _wait_ready(self) -> None:
        """Wait until EEPROM status register indicates ready status"""
        while True:
            received = self.spi.transceive(SpiPeripheral.EEPROM, [INSTR_RDSR, 0])
            if not (received[1] & 0x1):
                return


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


def parse_dec_or_hex_number(s: str) -> int:
    s = s.strip()
    if s.startswith("0x"):
        return int(s, 16)
    else:
        return int(s, 10)


def create_config(args: argparse.Namespace) -> Config:
    if args.erase and args.write:
        raise CommError("cannot write and erase at the same time")

    try:
        address = parse_dec_or_hex_number(args.address)
    except ValueError:
        raise CommError("invalid start address")
    if address < 0 or address >= EEPROM_SIZE:
        raise CommError("start address out of bounds")

    try:
        size = parse_dec_or_hex_number(args.size)
    except ValueError:
        raise CommError("invalid size")
    if size <= 0 or size > EEPROM_SIZE:
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
        input_data = []

    # prevent wrapping by limiting size to not pass the end of EEPROM
    size = min(size, EEPROM_SIZE - address)

    # if writing read data to std out, don't write status messages
    verbose = not args.read or args.output != STD_IO

    return Config(args.read, args.write, args.erase, address, size,
                  input_data, args.output, verbose)


def execute_config(driver: EepromDriver, config: Config) -> None:
    # read first, in any case
    read_data = driver.read(config.address, config.size,
                            print_progress_bar("Reading", 9, 30) if config.verbose else None)
    if config.read:
        if config.output_file == STD_IO:
            sys.stdout.buffer.write(read_data)
        else:
            with open(config.output_file, "wb") as file:
                file.write(read_data)

    if config.write:
        count = len(config.input_data)
        driver.write(config.address, count, config.input_data, read_data,
                     print_progress_bar("Writing", 9, 30) if config.verbose else None)

        # reread the written range and validate data
        # if verification fails just abort operation
        read_data = driver.read(config.address, count,
                                print_progress_bar("Verifying", 9, 30) if config.verbose else None)
        if read_data != config.input_data:
            raise CommError("EEPROM verification failed")

    elif config.erase:
        # "erasing" is just a facility for writing 0xff everywhere...
        data = bytearray()
        for i in range(config.size):
            data.append(ERASE_BYTE)
        driver.write(config.address, config.size, data, read_data,
                     print_progress_bar("Erasing", 9, 30) if config.verbose else None)
