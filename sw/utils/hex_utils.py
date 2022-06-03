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
from typing import Sequence

from utils import PathLike

# number of bytes per line when writing HEX files
HEX_WRITER_BLOCK_SIZE = 16


def read_hex_file(filename: PathLike, max_size: int) -> bytes:
    """Partial implementation of an intel HEX reader to read the program data.
    Note that the offset from address 0 is removed."""
    data = bytearray()
    offset = 0
    start_address = -1
    with open(filename, "r") as file:
        for line in file:
            line = line.strip()
            if not line.startswith(":"):
                continue

            content = bytes.fromhex(line[1:])
            length = content[0]
            address = (content[2] | content[1] << 8) + offset
            if start_address == -1:
                start_address = address
            record = content[3]
            if record == 0x00:
                # data
                if address >= max_size:
                    continue
                if address - start_address > len(data):
                    data += bytearray(address - start_address - len(data))
                data.extend(content[4:4 + length])
            elif record == 0x01:
                # end of file
                break
            elif record == 0x02:
                # extended segment address
                offset = (content[5] | content[4] << 8) << 4
            elif record == 0x04:
                # extended linear address
                offset = (content[5] | content[4] << 8) << 16
    return data


def write_hex_file(filename: PathLike, data: bytes) -> None:
    """Partial implementation of an intel HEX writer to write program data.
    Note that the data is always written from address 0."""
    with open(filename, "w") as file:
        def write_line(record_type: int, address: int, data: Sequence[int]) -> None:
            file.write(f":{len(data):02X}{address >> 8:02X}{address & 0xff:02X}{record_type:02X}")
            checksum = len(data) + (address >> 8) + (address & 0xff) + record_type
            for b in data:
                file.write(f"{b:02X}")
                checksum += b
            file.write(f"{-checksum & 0xff:02X}\n")

        for i in range(0, len(data), HEX_WRITER_BLOCK_SIZE):
            length = min(HEX_WRITER_BLOCK_SIZE, len(data) - i)
            write_line(0x00, i, data[i:i + length])  # data
        write_line(0x01, 0, [])  # end of file
