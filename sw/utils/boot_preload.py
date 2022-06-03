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

# This utility is used to output a single HEX file from the bootloader HEX and an app HEX.
# The resulting file contains both programs and can be flashed to the microcontroller.
# This allows to circumvent the bootloader mechanism and preload an app directly.
#
# The use case for this is when a new bootloader version is made, all apps on the flash use the
# old version, so no apps will be appear in the list. The system app is required to program the
# flash, so if it was not updated BEFORE the bootloader, the device cannot be programmed anymore.
# Preloading the system app can fix this.
#
# Usage:
#
#     ./boot_preload.py <target-dir> [output-hex-file]
#
# <target-dir>: directory of the app to preload, relative to the sw/ directory.
# <output-hex-file>: output HEX file, `output.hex` by default.
#
# This script should only be run from the sw/ directory.

import argparse
from pathlib import Path

from hex_utils import read_hex_file, write_hex_file

BOOTLOADER_SIZE = 8192


class PreloadError(Exception):
    pass


def main() -> None:
    args = parser.parse_args()

    boot_hex = Path("boot/build/avr/main.hex")
    if not boot_hex.exists():
        raise PreloadError(f"could not find bootloader HEX ({boot_hex}, "
                           f"make sure to compile it first.")

    app_hex = Path(args.target, "build/avr/main.hex")
    if not boot_hex.exists():
        raise PreloadError(f"could not find app HEX ({app_hex}), "
                           f"make sure to compile it first.")

    print("Reading boot HEX")
    boot_code = read_hex_file(boot_hex, 0x10000)
    print("Reading app HEX")
    app_code = read_hex_file(app_hex, 0x10000)

    print("Writing output HEX")
    preload_code = bytearray(BOOTLOADER_SIZE + len(app_code))
    preload_code[:len(boot_code)] = boot_code
    for i in range(len(boot_code), BOOTLOADER_SIZE):
        preload_code[i] = 0xff
    preload_code[BOOTLOADER_SIZE:] = app_code
    write_hex_file(args.output, preload_code)
    print(f"DONE, output to '{args.output}'")


parser = argparse.ArgumentParser(description="App preloading utility")
parser.add_argument(
    "target", action="store", type=str,
    help="App target directory, relative to the sw/ directory.")
parser.add_argument(
    "output", action="store", type=str, default="output.hex", nargs="?",
    help=f"Output file. Default is 'output.hex'.")

if __name__ == '__main__':
    try:
        main()
    except PreloadError as ex:
        print(f"ERROR: {ex}.")
        exit(1)
