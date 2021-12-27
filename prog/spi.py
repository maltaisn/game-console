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
from enum import Enum
from typing import Sequence

from comm import CommInterface, PacketType, Packet, CommError
from utils import ProgressCallback


class SpiPeripheral(Enum):
    """All SPI peripherals"""
    FLASH = 0b00
    EEPROM = 0b01
    DISPLAY = 0b10
    NONE = -1


class SpiInterface:
    """Class used to communicate on the SPI bus."""
    comm: CommInterface
    last_transfer: bool

    # maximum size of transfer per SPI packet
    MAX_TRANSFER_SIZE = 254

    def __init__(self, comm: CommInterface):
        self.comm = comm
        self._peripheral = SpiPeripheral.NONE
        self.last_transfer = True

    @property
    def peripheral(self) -> SpiPeripheral:
        return self._peripheral

    @peripheral.setter
    def peripheral(self, value: SpiPeripheral):
        if value != self._peripheral:
            if not self.last_transfer:
                # this is important to avoid having two CS line asserted at once.
                raise ValueError("SPI peripheral changed but last transfer didn't have last flag")
            self._peripheral = value

    def transceive(self, data: Sequence[int], last: bool = True,
                   progress: ProgressCallback = None) -> bytes:
        """
        Transmit and receive data with peripheral on SPI bus.
        A flag indicates the last transfer. This handles transfer larger than maximum allowed
        per SPI packet smoothly by splitting into multiple packets.
        """
        if self.peripheral == SpiPeripheral.NONE:
            raise RuntimeError("No SPI peripheral set before transfer")
        self.last_transfer = last

        # if transfer is larger than maximum allowed by packet, split into multiple packets.
        pos = 0
        read = bytearray()
        while pos < len(data):
            # find length of next packet and if it's the last
            length = len(data) - pos
            last_part = last
            if length > SpiInterface.MAX_TRANSFER_SIZE:
                length = SpiInterface.MAX_TRANSFER_SIZE
                last_part = False

            # create packet & send it
            payload_tx = bytearray()
            payload_tx.append(self.peripheral.value | (0x80 if last_part else 0x00))
            payload_tx += bytes(data[pos:pos + length])
            self.comm.write(Packet(PacketType.SPI, payload_tx))
            payload_rx = self.comm.read(PacketType.SPI).payload
            read += payload_rx[1:]
            if len(payload_rx) != len(payload_tx):
                raise CommError("short SPI packet")
            pos += length

            if progress:
                progress(pos, len(data))

        return read
