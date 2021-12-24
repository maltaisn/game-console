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
from typing import Sequence, Optional, Callable

from comm import CommInterface, PacketType, Packet
from utils import ProgressCallback


class SpiPeripheral(Enum):
    """All SPI peripherals"""
    FLASH = 0b00
    EEPROM = 0b01
    DISPLAY = 0b10


class SpiInterface:
    """Class used to communicate on the SPI bus."""
    comm: CommInterface
    last_peripheral: Optional[SpiPeripheral]
    last_transfer: bool

    # maximum size of transfer per SPI packet
    MAX_TRANSFER_SIZE = 254

    def __init__(self, comm: CommInterface):
        self.comm = comm
        self.last_peripheral = None
        self.last_transfer = True

    def transceive(self, per: SpiPeripheral, data: Sequence[int], last: bool = True,
                   progress: ProgressCallback = None) -> bytes:
        """
        Transmit and receive data with peripheral on SPI bus.
        A flag indicates the last transfer. This handles transfer larger than maximum allowed
        per SPI packet smoothly by splitting into multiple packets.
        """
        if not self.last_transfer and per != self.last_peripheral:
            # this is important to avoid having two CS line asserted at once.
            raise ValueError("SPI peripheral changed but last transfer didn't have last flag")

        self.last_peripheral = per
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
            payload = bytearray()
            payload.append(per.value | (0x80 if last_part else 0x00))
            payload += bytes(data[pos:pos + length])
            self.comm.write(Packet(PacketType.SPI, payload))
            read += self.comm.read(len(payload)).payload[1:]
            pos += length

            if progress:
                progress(pos, len(data))

        return read
