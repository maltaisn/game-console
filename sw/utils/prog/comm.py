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
import socket
from dataclasses import dataclass
from enum import Enum
from typing import Optional, Sequence, Union

import serial
from serial import Serial


class ProgError(Exception):
    pass


class PacketType(Enum):
    """Packet types defined by firmware."""
    VERSION = 0x00
    SPI = 0x01
    LOCK = 0x02
    SLEEP = 0x03
    BATTERY_INFO = 0x10
    BATTERY_CALIB = 0x11
    BATTERY_LOAD = 0x12


@dataclass
class Packet:
    """Packet received and transmitted to firmware."""
    packet_type: int
    payload: bytes

    SIGNATURE_BYTE = 0x73
    PACKET_MAX_SIZE = 256
    PACKET_HEADER_SIZE = 3
    PAYLOAD_MAX_SIZE = PACKET_MAX_SIZE - PACKET_HEADER_SIZE

    def __init__(self, packet_type: Union[PacketType, int], payload: Sequence[int] = None):
        self.packet_type = packet_type if isinstance(packet_type, int) else packet_type.value
        self.payload = bytes(payload) if payload else bytes()

    def encode(self) -> bytes:
        if len(self.payload) > Packet.PAYLOAD_MAX_SIZE:
            raise ProgError("packet payload size exceeds maximum")
        packet = bytearray()
        packet.append(Packet.SIGNATURE_BYTE)
        packet.append(self.packet_type)
        packet.append(len(self.payload) + Packet.PACKET_HEADER_SIZE - 1)
        packet += self.payload
        return packet

    @staticmethod
    def decode(data: Sequence[int]) -> "Packet":
        if data[0] != Packet.SIGNATURE_BYTE:
            raise ProgError("packet signature is invalid")
        packet_type = data[1]
        length = max(Packet.PACKET_HEADER_SIZE, data[2] + 1)
        payload = bytes(data[Packet.PACKET_HEADER_SIZE:length])
        return Packet(packet_type, payload)


class SimulatorSerial:
    SOCKET_NAME = "/tmp/gcsim"

    socket: socket.socket

    def __init__(self):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
        self.socket.connect(SimulatorSerial.SOCKET_NAME)

    def read(self, n: int) -> bytes:
        received = bytearray()
        while len(received) < n:
            received += self.socket.recv(n - len(received))
        return received

    def write(self, data: bytes) -> None:
        self.socket.send(data)

    def read_all(self) -> None:
        pass  # not implemented

    def flush(self) -> None:
        pass  # no-op

    def close(self) -> None:
        self.socket.close()


class CommInterface(abc.ABC):
    def write(self, packet: Packet) -> None:
        """Write a packet to the communication interface."""
        raise NotImplementedError

    def read(self) -> Packet:
        """Read a packet from the communication interface (optionally a specific packet type)."""
        raise NotImplementedError


class Comm(CommInterface):
    """Class used for communication with firmware. Uses pyserial to send and receive packets."""
    filename: str
    baud_rate: int
    simulator: bool
    serial: Optional[Union[Serial, SimulatorSerial]]

    # Used to enforce the "read after write" and "write before read" recommandations
    # of the system UART communication interface to avoid losing packets.
    _written: Optional[int]

    DEFAULT_FILENAME = "/dev/ttyACM1"
    DEFAULT_BAUD_RATE = 250_000

    def __init__(self, filename: str = DEFAULT_FILENAME,
                 baud_rate: int = DEFAULT_BAUD_RATE,
                 simulator: bool = False):
        self.filename = filename
        self.baud_rate = baud_rate
        self.serial = None
        self.simulator = simulator
        self._written = None

    def connect(self) -> None:
        try:
            if self.simulator:
                # connect to simulator socket
                self.serial = SimulatorSerial()
            else:
                # connect to actual device
                self.serial = Serial(
                    port=self.filename,
                    baudrate=self.baud_rate,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    bytesize=serial.EIGHTBITS,
                    timeout=0.1,
                )
                self.serial.read_all()  # discard content from before program started

        except IOError as e:
            raise ProgError("could not connect to device") from e

    def disconnect(self) -> None:
        if self.is_connected():
            self.serial.close()

    def is_connected(self) -> bool:
        return self.serial is not None

    def write(self, packet: Packet) -> None:
        if self._written is not None:
            raise RuntimeError("no read after write")
        self.serial.write(packet.encode())
        self._written = packet.packet_type

    def read(self) -> Packet:
        if self._written is None:
            raise RuntimeError("read without write")

        while True:
            # find signature byte
            signature = self.serial.read(1)
            if len(signature) != 1:
                raise ProgError("incomplete packet")
            if signature[0] != Packet.SIGNATURE_BYTE:
                continue

            header = self.serial.read(2)
            if len(header) != 2:
                raise ProgError("incomplete packet")
            payload_length = max(0, header[1] - Packet.PACKET_HEADER_SIZE + 1)

            payload = self.serial.read(payload_length)
            if len(payload) != payload_length:
                raise ProgError("incomplete packet")
            packet = Packet.decode(signature + header + payload)
            break

        if packet.packet_type != self._written:
            raise ProgError("unexpected packet type")
        self._written = None
        return packet
