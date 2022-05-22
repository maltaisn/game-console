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
from dataclasses import dataclass, field
from enum import Enum

from utils import readable_size


class PackError(Exception):
    pass


class Location(Enum):
    INTERNAL = "mcu"
    FLASH = "flash"


@dataclass(frozen=True)
class PackResult:
    """Class used to return packed data, with a __repr__ implementation used to describe it."""
    data: bytes = field(repr=False)

    def __repr__(self) -> str:
        return readable_size(len(self.data))


@dataclass(frozen=True)
class DataObject(abc.ABC):
    """A data object that can be packed to a byte array."""

    def pack(self) -> PackResult:
        """Returns a pack result containing binary data for the object.
        Any error in the values at this point should be handled by raising a PackError."""
        raise NotImplementedError

    def is_in_unified_data_space(self) -> bool:
        """Whether the object address should be given in unified data space."""
        return False

    def get_type_name(self) -> str:
        """Returns a lowercase string indicating the type of this data object."""
        raise NotImplementedError
