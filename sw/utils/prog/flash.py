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

# This module is used to read, write & erase the flash chip on the game console
# The flash chip model number is AT25SF081B.
# Datasheet: https://www.dialog-semiconductor.com/sites/default/files/at25sf081b.pdf
# Flash devices support SFDP which could allow this driver to work for any flash device, but parsing
# the SFDP table is more tedious than just hardcoding the values for a specific chip.

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Union

from prog.comm import ProgError
from prog.memory import MemoryDriver, MemoryLocal
from prog.spi import SpiInterface, SpiPeripheral
from utils import ProgressCallback, process_bitarray_sequences
from bitarray import bitarray

FLASH_SIZE = 1048576
# flash page size in bytes
PAGE_SIZE = 256
ADDRESS_BYTES = 3

# driver will abort the operation if the flash chip manufacturer & device ID
# does not match this value (corresponds to the 3 bytes returned by command 0x9f).
MANUFACTURER_ID = 0x01851f


@dataclass(frozen=True)
class EraseInstruction:
    opcode: int
    typ_time: float


SMALLEST_BLOCK_SIZE = 4096

# instruction set
INSTR_READ = 0x03
INSTR_WRITE = 0x02
INSTR_ERASE = {
    SMALLEST_BLOCK_SIZE: EraseInstruction(0x20, 0.06),
    32768: EraseInstruction(0x52, 0.135),
    65536: EraseInstruction(0xd8, 0.220),
    FLASH_SIZE: EraseInstruction(0x60, 3),
}
INSTR_WRITE_EN = 0x06
INSTR_READ_STATUS = 0x05
INSTR_RESET_ENABLE = 0x66
INSTR_RESET = 0x99
INSTR_MANUF_ID = 0x9f


class FlashDriver(MemoryDriver):
    """Class used to read & write to the flash chip."""
    spi: SpiInterface

    def __init__(self, spi: SpiInterface):
        spi.peripheral = SpiPeripheral.FLASH
        self.spi = spi

    def reset(self) -> None:
        """Reset device and verify manufacturer ID."""
        self._wait_ready()
        self.spi.transceive([INSTR_RESET_ENABLE])
        self.spi.transceive([INSTR_RESET])
        id_data = self.spi.transceive([INSTR_MANUF_ID, 0, 0, 0])
        id_num = id_data[1] | id_data[2] << 8 | id_data[3] << 16
        if id_num != MANUFACTURER_ID:
            raise ProgError("Flash device manufacturer and device ID do not match predefined value")

    def get_size(self) -> int:
        return FLASH_SIZE

    def requires_block_erase(self) -> bool:
        return True

    def get_smallest_erase_size(self) -> int:
        return SMALLEST_BLOCK_SIZE

    def get_block_count(self) -> int:
        return FLASH_SIZE // SMALLEST_BLOCK_SIZE

    def read(self, address: int, count: int, progress: ProgressCallback) -> bytes:
        self._wait_ready()
        command = bytearray([INSTR_READ])
        command += address.to_bytes(3, "big", signed=False)
        # insert dummy MOSI bytes
        for i in range(count):
            command.append(0)
        received = self.spi.transceive(command, progress=lambda c, t: progress(c - 4, t - 4))
        return received[4:]

    def write(self, address: int, data: bytes, progress: ProgressCallback) -> None:
        """Write bytes to flash at an address.
        All bytes written must be in erased state (0xff) beforehand."""
        if address % PAGE_SIZE != 0 or len(data) % PAGE_SIZE != 0:
            raise ValueError("Write address and data size must be page aligned!")
        pos = 0
        self._wait_ready()
        while pos < len(data):
            self._write_enable()
            # write data for one page
            command = bytearray([INSTR_WRITE])
            command += address.to_bytes(ADDRESS_BYTES, "big", signed=False)
            for i in range(PAGE_SIZE):
                command.append(data[pos])
                pos += 1
            self.spi.transceive(command)
            self._wait_ready()
            progress(pos, len(data))
            address += PAGE_SIZE

    def erase_blocks(self, blocks: bitarray, progress: ProgressCallback) -> None:
        # for each sequence, erase with the largest possible block since it's faster.
        # however the erase block start address must be aligned to the block size.
        # estimate progress using typical erase times given in datasheet.
        self._wait_ready()

        def erase_process(start: int, end: int, _) -> None:
            nonlocal erased
            while start != end:
                for block_size in reversed(INSTR_ERASE):
                    if start % block_size == 0 and end - start >= block_size:
                        # this is the largest block erase possible from current position
                        self._write_enable()
                        instr = INSTR_ERASE[block_size]
                        command = bytearray([instr.opcode])
                        if block_size != FLASH_SIZE:
                            # erase instructions require an address, except chip erase
                            command += start.to_bytes(ADDRESS_BYTES, "big", signed=False)
                        self.spi.transceive(command)
                        # estimate progress using typical datasheet erase time
                        # blocks at last byte if not done after that time.
                        start_time = time.time()
                        while not self._is_ready():
                            elapsed = time.time() - start_time
                            block_erased_est = min(block_size - 1,
                                                   round(block_size * elapsed / instr.typ_time))
                            progress(erased + block_erased_est, count)
                        erased += block_size
                        progress(erased, count)
                        start += block_size
                        break

        erased = 0
        count = blocks.count() * SMALLEST_BLOCK_SIZE
        process_bitarray_sequences(self, blocks, "", erase_process, verbose=False)

    def erase(self, progress: ProgressCallback) -> None:
        """Erase whole device."""
        all_blocks = bitarray(self.get_block_count())
        all_blocks.setall(True)
        self.erase_blocks(all_blocks, progress)

    def _write_enable(self) -> None:
        self.spi.transceive([INSTR_WRITE_EN])

    def _is_ready(self) -> bool:
        """Returns true if flash device is ready."""
        received = self.spi.transceive([INSTR_READ_STATUS, 0])
        return not (received[1] & 0x1)

    def _wait_ready(self) -> None:
        """Wait until flash device is ready."""
        while not self._is_ready():
            pass

    @staticmethod
    def local(filename: Union[str, Path]) -> MemoryLocal:
        return MemoryLocal(filename, FLASH_SIZE, 0xff, SMALLEST_BLOCK_SIZE, True)
