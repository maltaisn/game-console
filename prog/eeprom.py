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
import itertools
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from comm import CommError
from spi import SpiInterface, SpiPeripheral

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

    def read(self, start: int, count: int) -> bytes:
        self._wait_ready()
        data = bytearray()
        command = [INSTR_READ, (start >> 8) & 0xff, start & 0xff]
        # insert dummy MOSI bytes
        for i in range(count):
            command.append(0)
        received = self.spi.transceive(SpiPeripheral.EEPROM, command)
        return received[3:]

    def write(self, start: int, count: int, data: Iterable[int]) -> None:
        written = 0
        address = start
        it = iter(data)
        while written < count:
            # wait until ready and enable write latch
            self._wait_ready()
            self.spi.transceive(SpiPeripheral.EEPROM, [INSTR_WREN])
            # write data for one page
            command = [INSTR_WRITE, (address >> 8) & 0xff, address & 0xff]
            page_end = (address & ~0x1f) + PAGE_SIZE
            while written < count and address < page_end:
                byte = next(it, None)
                if byte is None:
                    break
                command.append(byte)
                written += 1
                address += 1
            self.spi.transceive(SpiPeripheral.EEPROM, command)

    def erase(self) -> None:
        self.write(0x0000, EEPROM_SIZE, itertools.repeat(ERASE_BYTE))

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
    if size < 0 or size > EEPROM_SIZE:
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
    else:
        input_data = []

    # prevent wrapping by limiting size to not pass the end of EEPROM
    size = min(size, EEPROM_SIZE - address)

    # if writing read data to std out, don't write status messages
    verbose = not args.read or args.output != STD_IO

    return Config(args.read, args.write, args.erase, address, size,
                  input_data, args.output, verbose)


def execute_config(driver: EepromDriver, config: Config) -> None:
    # read first, then write or erase
    if config.read:
        start_time = time.time()
        read_data = driver.read(config.address, config.size)
        end_time = time.time()
        if config.verbose:
            print(f"Read {config.size} bytes from EEPROM in {end_time - start_time:.2f} s")
        if config.output_file == STD_IO:
            sys.stdout.buffer.write(read_data)
        else:
            with open(config.output_file, "wb") as file:
                file.write(read_data)

    if config.write:
        count = len(config.input_data)
        start_time = time.time()
        driver.write(config.address, count, config.input_data)
        end_time = time.time()
        if config.verbose:
            print(f"Written {count} bytes to EEPROM in {end_time - start_time:.2f} s")

        # reread the written range and validate data
        # if verification fails just abort operation
        start_time = time.time()
        read_data = driver.read(config.address, count)
        end_time = time.time()
        if read_data != config.input_data:
            raise CommError("EEPROM verification failed")
        elif config.verbose:
            print(f"Succesfully verified data in {end_time - start_time:.2f} s")

    elif config.erase:
        start_time = time.time()
        driver.erase()
        end_time = time.time()
        print(f"Erased EEPROM in {end_time - start_time:.2f} s")
