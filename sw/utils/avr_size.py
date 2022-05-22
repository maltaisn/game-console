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

# Existing solution for reporting the size of the compiled code don't work with the
# custom linker scripts used in this application. A custom solution is needed to do so.
# This simply parses the linker map file to extract addresses and report usage size
# A custom linker script is required, some symbols would be missing otherwise.
#
# Usage:
#
#   $ ./avr_size.py <linker_map_file>
#

import re
import sys


class MapFileError(Exception):
    pass


def main() -> None:
    if len(sys.argv) != 2:
        raise MapFileError("wrong number of arguments")

    try:
        with open(sys.argv[1], "r") as file:
            content = file.read()
    except IOError as e:
        raise MapFileError(f"Map file couldn't be opened: {e}")

    def get_symbol_value(name: str) -> int:
        match = re.search(r"0x([a-f\d]+)]?\s+" + name, content)
        if not match:
            raise MapFileError(f"Symbol not found: {name}")
        return int(match.group(1)[2:], 16)

    def get_section_size(start: str, end: str) -> int:
        return get_symbol_value(end) - get_symbol_value(start)

    def print_section_usage(name: str, usage: int, size: int) -> None:
        print(f"{name:<6}   {usage:>7} B   {size:>9} B   {usage / size:>6.1%}")

    print("\n=========================================")
    print("Region   Used size   Region size   % used")

    ram_size = get_symbol_value("__TARGET_DATA_LENGTH__")
    ram_usage = get_section_size("__ram_start", "__ram_end")
    print_section_usage("RAM", ram_usage, ram_size)

    text_size = get_section_size("__text_start", "__text_end")
    rodata_size = get_section_size("__rodata_start", "__rodata_end")
    data_load_size = get_section_size("__data_load_start", "__data_load_end")

    flash_size = get_symbol_value("__TARGET_TEXT_LENGTH__")
    flash_usage = text_size + rodata_size + data_load_size
    print_section_usage("Flash", flash_usage, flash_size)

    print("=========================================\n")


if __name__ == '__main__':
    try:
        main()
    except MapFileError as e:
        print(f"Error calculating size: {e}")
        exit(1)
