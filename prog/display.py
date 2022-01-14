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

# This module is used to read, write & erase the flash chip on the game console
# The flash chip model number is AT25SF081B.
# Datasheet: https://www.dialog-semiconductor.com/sites/default/files/at25sf081b.pdf
# Flash devices support SFDP which could allow this driver to work for any flash device, but parsing
# the SFDP table is more tedious than just hardcoding the values for a specific chip.
import random
import time
from typing import Sequence

from comm import CommInterface, Packet, PacketType
from spi import SpiInterface, SpiPeripheral


class DisplayInterface:
    spi: SpiInterface
    comm: CommInterface

    PIN_DC = 1 << 0
    PIN_RES = 1 << 1

    def __init__(self, comm: CommInterface):
        self.comm = comm
        self.spi = SpiInterface(comm)
        self.spi.peripheral = SpiPeripheral.DISPLAY

    def reset(self) -> None:
        self.comm.write(Packet(PacketType.IO, [DisplayInterface.PIN_RES]))
        time.sleep(0.01)
        self.comm.write(Packet(PacketType.IO, [0]))
        time.sleep(0.01)
        self.comm.write(Packet(PacketType.IO, [DisplayInterface.PIN_RES]))
        time.sleep(0.01)

    def write_data(self, data: Sequence[int]) -> None:
        self.comm.write(Packet(PacketType.IO, [DisplayInterface.PIN_DC | DisplayInterface.PIN_RES]))
        self.spi.transceive(data)

    def write_command(self, data: Sequence[int]) -> None:
        self.comm.write(Packet(PacketType.IO, [DisplayInterface.PIN_RES]))
        self.spi.transceive(data)

class DisplayDriver:
    """Class used to interact with the display."""
    interface: DisplayInterface

    GPIO_DISABLE = 0b00
    GPIO_INPUT = 0b01
    GPIO_OUTPUT_LO = 0b10
    GPIO_OUTPUT_HI = 0b11

    _INIT_SEQUENCE = [
        0xae,        # display OFF
        0xa0, 0x43,  # remap flags
        0xab, 0x01,  # internal VDD
        0xb1, 0x31,  # phase length
        0xb3, 0xb1,  # oscillator
        0xb6, 0x0d,  # precharge period
        0xbc, 0x07,  # precharge voltage
        0xd5, 0x02,  # enable external VSL
        0xbe, 0x07,  # VCOM voltage
        0x81, 0x7f,  # default contrast
        0xb5, 0x03,  # GPIO high (enable 15V reg)
        0xaf,        # display ON
    ]

    _DEINIT_SEQUENCE = [
        0xae,        # display OFF
        0xb5, 0x02,  # GPIO low (disable 15V reg)
    ]

    def __init__(self, interface: DisplayInterface):
        self.interface = interface

    def init(self) -> None:
        self.interface.reset()
        self.interface.write_command(DisplayDriver._INIT_SEQUENCE)

    def deinit(self) -> None:
        self.interface.write_command(DisplayDriver._DEINIT_SEQUENCE)

    def do_test(self) -> None:
        for c in range(16):
            data = bytearray(8192)
            for i in range(len(data)):
                data[i] = c | c << 4
            self.interface.write_data(data)
            time.sleep(0.5)
