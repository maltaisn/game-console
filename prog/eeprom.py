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

# This module is used to read, write & erase the EEPROM chip on the game console
# The EEPROM chip model number is FT25C32A
# Datasheet: https://www.fremontmicro.com/downfile.aspx?filepath=/upload/2019/0805/1604ntvhj2.pdf&filename=ft25c32a_ds_rev0.82.pdf

from memory import MemoryDriver, DiffSequences
from spi import SpiInterface, SpiPeripheral
from utils import ProgressCallback

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


class EepromDriver(MemoryDriver):
    """Class used to read & write to the EEPROM chip."""
    spi: SpiInterface

    def __init__(self, spi: SpiInterface):
        spi.peripheral = SpiPeripheral.EEPROM
        self.spi = spi

    def read(self, address: int, count: int, progress: ProgressCallback) -> bytes:
        self._wait_ready()
        command = [INSTR_READ, address >> 8, address & 0xff]
        # insert dummy MOSI bytes
        for i in range(count):
            command.append(0)
        received = self.spi.transceive(command, progress=lambda c, t: progress(c - 3, t - 3))
        return received[3:]

    def _write(self, address: int, data: bytes, progress: ProgressCallback) -> None:
        """Write bytes to EEPROM at an address."""
        # wait until ready and enable write latch
        written = 0
        it = iter(data)
        while written < len(data):
            self._wait_ready()
            self.spi.transceive([INSTR_WREN])
            # write data for one page
            command = [INSTR_WRITE, address >> 8, address & 0xff]
            page_end = (address & ~(PAGE_SIZE - 1)) + PAGE_SIZE
            while written < len(data) and address < page_end:
                byte = next(it, None)
                if byte is None:
                    break
                command.append(byte)
                written += 1
                address += 1
            self.spi.transceive(command)
            progress(written, len(data))

    def write(self, address: int, data: bytes, count: int, sequences: DiffSequences,
              progress: ProgressCallback) -> None:
        # write new data in identified sequences
        written = 0

        def progress_delegate(c: int, _) -> None:
            return progress(written + c, count)

        for start, end in sequences:
            self._write(start + address, data[start:end], progress_delegate)
            written += end - start

    def requires_block_erase(self) -> bool:
        return False

    def erase(self, progress: ProgressCallback) -> None:
        data = bytearray()
        for i in range(EEPROM_SIZE):
            data.append(ERASE_BYTE)
        self.write(0, data, EEPROM_SIZE, [(0, EEPROM_SIZE)], progress)

    def _wait_ready(self) -> None:
        """Wait until status register indicates ready status"""
        while True:
            received = self.spi.transceive([INSTR_RDSR, 0])
            if not (received[1] & 0x1):
                return
