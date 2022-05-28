#!/usr/bin/env python3

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

# This script is used to pack the app configuration, compiled code, and packed assets
# into a single "app image" file that can be used by gcprog to install the app on the device.
#
# Usage:
#
#   $ ./app_packer.py <target_path>
#
# The current directory must be sw/ and the target path is relative to it.

import re
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict

import assets_packer
from prog.app import App, DataLocation
from utils import readable_size, PathLike, boot_crc16

# filename for the target configuration file
TARGET_CONF_FILE = "target.cfg"
# configuration file for the boot target
BOOT_TARGET_CONF_FILE = "boot/target.cfg"

# filename containing packed assets stored in flash
PACKED_ASSETS_FILE = assets_packer.ASSETS_FILE

# output filename for this script
PACKED_APP_FILE = "target.app"


class PackError(Exception):
    pass


@dataclass(frozen=True)
class TargetConfig:
    app_id: int
    version: int
    title: str
    author: str
    eeprom_space: int
    page_height: int


def read_hex_file(filename: PathLike, max_size: int) -> bytes:
    """Partial implementation of an intel HEX reader to read the program data."""
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


def read_config_file(filename: PathLike) -> Dict[str, str]:
    """Read a .cfg file and extract all fields into a string dictionnary."""
    config = {}
    with open(filename, "r") as file:
        for line in file:
            match = re.match(r"^\s*(\w+)\s*=\s*(.*?)\s*(?:#.*)?$", line)
            if match:
                config[match.group(1)] = match.group(2)
    return config


def read_target_config(target_path: Path) -> TargetConfig:
    """Read a 'target.cfg' file used for defining configuration properties for an app."""
    config_file = target_path / TARGET_CONF_FILE
    config = read_config_file(config_file)
    try:
        app_id = int(config["id"], 0)
        version = int(config["version"], 0)
        title = config["title"]
        author = config["author"].upper()
        eeprom_space = int(config.get("eeprom_space", "0"), 0)
        page_height = int(config["display_page_height"], 0)
    except KeyError as e:
        raise PackError(f"undefined value for {e} in {config_file}")
    except ValueError:
        raise PackError(f"unexpected value for field in {config_file}")

    # validate configuration
    if not (0x01 <= app_id <= 0xff):
        raise PackError("app ID must be between 0x01 and 0xff")
    if not (0x0000 <= version <= 0xffff):
        raise PackError("version must be between 0 and 0xffff")
    if not re.match(r"^[A-Za-z\d\-_ .]{1,15}$", title):
        raise PackError("title has an invalid format")
    if not re.match(r"^[A-Z\d\-_ .]{1,15}$", author):
        raise PackError("author has an invalid format")
    if not (0 <= eeprom_space <= 0xffff):
        raise PackError(f"EEPROM space out of bounds")
    if not (0 <= page_height <= 128):
        raise PackError("display page height out of bounds")

    return TargetConfig(app_id, version, title, author, eeprom_space, page_height)


def read_boot_version() -> int:
    """Read bootloader version from its configuration file."""
    if Path(BOOT_TARGET_CONF_FILE).exists():
        config = read_config_file(BOOT_TARGET_CONF_FILE)
        boot_version = int(config.get("boot_version", "-1"), 0)
        if boot_version == -1:
            raise PackError("boot version not specified in configuration file")
        if not (0x0000 <= boot_version <= 0xffff):
            raise PackError("boot version must be between 0 and 0xffff")
        return boot_version

    raise PackError(f"could not determine boot version (from {BOOT_TARGET_CONF_FILE})")


def read_app_code(target_path: Path) -> bytes:
    """Read app code from compiled AVR target HEX file."""
    # check if build dir exists
    build_dir = target_path / "build/avr"
    if not build_dir.exists():
        raise PackError(f"build directory doesn't exist ({build_dir}). "
                        f"Make sure to compile the app first")

    # read app code from compiled hex file
    app_hex_file = build_dir / "main.hex"
    if not app_hex_file.exists():
        raise PackError("app HEX file doesn't exist")

    return read_hex_file(app_hex_file, 0x10000)


def read_app_data(target_path: Path) -> bytes:
    """Read assets from pack file."""
    assets_file = target_path/ PACKED_ASSETS_FILE
    try:
        with open(assets_file, "rb") as file:
            return file.read()
    except IOError as e:
        raise PackError(f"could not read assets file ({assets_file}): {e}")


def main() -> None:
    if len(sys.argv) != 2:
        raise PackError("wrong number of arguments")
    target_path = Path(sys.argv[1])
    if not target_path.exists():
        raise PackError("target path doesn't exist")

    boot_version = read_boot_version()
    config = read_target_config(target_path)
    app_code = read_app_code(target_path)
    app_data = read_app_data(target_path)

    # pack the app as it is stored in flash (code + data)
    packed_app = bytearray()
    packed_app += app_code
    packed_app += app_data
    app_size = len(packed_app)

    # create the app object
    app_crc = boot_crc16(packed_app)
    code_crc = boot_crc16(app_code)

    app = App(config.app_id, app_crc, code_crc, config.version, boot_version, len(app_code),
              config.page_height, DataLocation(0, app_size), DataLocation(0, config.eeprom_space),
              datetime.today(), config.title, config.author)

    # write the app image (header + code + data), as read by gcprog.
    image_data = bytearray()
    image_data += App.GC_SIGNATURE
    image_data += app.encode()
    image_data += app_code
    image_data += app_data

    try:
        with open(target_path / PACKED_APP_FILE, "wb") as file:
            file.write(image_data)
    except IOError as e:
        raise PackError(f"could not write app file: {e}")

    print(f"DONE, total app size is {readable_size(app_size)}")


if __name__ == '__main__':
    try:
        main()
    except PackError as ex:
        print(f"ERROR: {ex}.")
