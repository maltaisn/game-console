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
from dataclasses import dataclass
from typing import Union

from assets.types import DataObject, PackResult, PackError

from tworld import Tile, Actor


@dataclass(frozen=True)
class TileHelpPackResult(PackResult):
    tile: Union[Tile, Actor]

    def __repr__(self) -> str:
        return super().__repr__() + \
               f", {('tile' if isinstance(self.tile, Tile) else 'actor')} " \
               f"'{repr(self.tile).lower()}'"


@dataclass(frozen=True)
class TileHelpObject(DataObject):
    tile: Union[Tile, Actor]
    name: str

    def pack(self) -> PackResult:
        data = bytearray()
        data.append(self.tile.value | (0 if isinstance(self.tile, Tile) else 0x80))
        data += self.name.encode("ascii")
        data.append(0)  # nul terminator
        return TileHelpPackResult(data, self.tile)

    def get_type_name(self) -> str:
        return "tile help"


def register_builder(packer) -> None:
    @packer.builder
    def tile_help(tile: Union[Tile, Actor], name: str):
        if len(name) > 13:
            raise PackError("tile help name too long (max 13 chars)")
        yield TileHelpObject(tile, name)
