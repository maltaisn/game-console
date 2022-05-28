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
from dataclasses import dataclass
from enum import Enum
from typing import Optional, Sequence, Union

import serial
from serial import Serial

import socket


class ProgError(Exception):
    pass


class PacketType(Enum):
    """Packet types defined by firmware."""
    VERSION = 0x00
    SPI = 0x01
    FAST_MODE = 0x02


@dataclass
class Packet:
    """Packet received and transmitted to firmware."""
    packet_type: int
    payload: bytes

    SIGNATURE_BYTE = 0xc3
    MAX_PAYLOAD_SIZE = 0xff

    def __init__(self, packet_type: Union[PacketType, int], payload: Sequence[int] = None):
        self.packet_type = packet_type if isinstance(packet_type, int) else packet_type.value
        self.payload = bytes(payload) if payload else bytes()

    def encode(self) -> bytes:
        if len(self.payload) > Packet.MAX_PAYLOAD_SIZE:
            raise ProgError("packet payload size exceeds maximum")
        packet = bytearray()
        packet.append(Packet.SIGNATURE_BYTE)
        packet.append(self.packet_type)
        packet.append(len(self.payload))
        packet += self.payload
        return packet

    @staticmethod
    def decode(data: Sequence[int]) -> "Packet":
        if data[0] != Packet.SIGNATURE_BYTE:
            raise ProgError("packet signature is invalid")
        type = data[1]
        length = data[2]
        payload = bytes(data[3:3 + length])
        return Packet(type, payload)


class SimulatorSerial:
    SOCKET_NAME = "/tmp/gcsim"

    socket: socket.socket

    def __init__(self):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
        self.socket.connect(SimulatorSerial.SOCKET_NAME)

    def read(self, n: int) -> bytes:
        received = bytearray()
        while len(received) < n:
            received += self.socket.recv(n)
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

    def read(self, expected_type: Optional[PacketType] = None) -> Packet:
        """Read a packet from the communication interface (optionally a specific packet type)."""
        raise NotImplementedError

class Comm(CommInterface):
    """Class used for communication with firmware. Uses pyserial to send and receive packets."""
    filename: str
    baud_rate: int
    baud_rate_fast: int
    simulator: bool
    serial: Optional[Union[Serial, SimulatorSerial]]

    DEFAULT_FILENAME = "/dev/ttyACM1"
    DEFAULT_BAUD_RATE = 9_600
    DEFAULT_BAUD_RATE_FAST = 500_000

    def __init__(self, filename: str = DEFAULT_FILENAME,
                 baud_rate: int = DEFAULT_BAUD_RATE,
                 baud_rate_fast: int = DEFAULT_BAUD_RATE_FAST,
                 simulator: bool = False):
        self.filename = filename
        self.baud_rate = baud_rate
        self.baud_rate_fast = baud_rate_fast
        self.serial = None
        self.simulator = simulator

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
                # set timeout in relation with normal baud rate (faster = smaller timeout)
                self.serial.timeout = max(0.5, 10000 / self.baud_rate)

        except IOError as e:
            raise ProgError("could not connect to device") from e

    def disconnect(self) -> None:
        if self.is_connected():
            self.serial.close()

    def is_connected(self) -> bool:
        return self.serial is not None

    def write(self, packet: Packet) -> None:
        self.serial.write(packet.encode())

    def read(self, expected_type: Optional[PacketType] = None) -> Packet:
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
            length = header[1]

            payload = self.serial.read(length)
            if len(payload) != length:
                raise ProgError("incomplete packet")
            packet = Packet.decode(signature + header + payload)
            if not expected_type or packet.packet_type == expected_type.value:
                return packet

    def set_fast_baud_rate(self, enabled: bool) -> None:
        """Enable or disable fast baud rate."""
        if self.is_connected():
            self.serial.baudrate = self.baud_rate_fast if enabled else self.baud_rate
