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
import math
import os
import re
from contextlib import contextmanager
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, List, Callable, Optional, Union, NoReturn, Dict, Set, Tuple, Generator, \
    Iterable

from PIL import Image
from assets import font_gen, image_gen, sound_gen
from assets.codegen import CodeGenerator
from assets.types import DataObject, PackResult, PackError, Location
# name of the file used to save packed assets data
from utils import PathLike, print_progress_bar, readable_size

# name of the generated packed assets file
ASSETS_FILE = "assets.dat"
# name of the generated header file
HEADER_FILE = "include/assets.h"
# name of the generated source file
SOURCE_FILE = "src/assets.c"

# name of the assets directory (that's the working directory when packing).
ASSETS_DIR = "assets"


class ArrayType(Enum):
    # Array with regularly spaced elements (address + single offset).
    # If elements have different sizes, padding is added between them.
    # Fastest array type, may use lots of internal memory used if elements aren't the same size.
    REGULAR = 0
    # Array with unevenly spaced elements (multiple addresses stored in internal memory).
    # As fast as regular, some internal memory used.
    INDEXED_ABS = 1
    # Array with unevenly spaced elements (address + multiple offsets stored in internal memory).
    # Slower than INDEXED_ABS, uses potentially less internal memory (may be equivalent).
    INDEXED_REL = 2
    # Array with unevenly spaced elements (index address + multiple addresses stored in flash).
    # Note that this array type is not supported if array is stored in internal memory.
    # Slowest, must read address as well as element from flash.
    INDEXED_ABS_FLASH = 3


@dataclass
class PackObject:
    name: str
    group: str
    location: Location
    data: DataObject

    # these fields are set during packing
    result: Optional[PackResult] = field(default=None)
    address: int = field(default=0)
    pad_after: int = field(default=0)


@dataclass(frozen=True)
class RawObject(DataObject):
    data: bytes
    unified_space: bool

    def pack(self) -> PackResult:
        return PackResult(self.data)

    def is_in_unified_data_space(self) -> bool:
        return self.unified_space

    def get_type_name(self) -> str:
        return "raw"


def register_builtin_builders(packer) -> None:
    @packer.builder
    def raw(data: Iterable[int], *, unified_space: bool = False):
        yield RawObject(data, unified_space)

    @packer.file_builder
    def raw_file(filename: Path, *, unified_space: bool = False):
        try:
            with open(filename, "rb") as file:
                yield RawObject(file.read(), unified_space)
        except IOError as e:
            raise PackError(f"could not read raw file '{filename}': {e}")

    @packer.builder
    def string(data: str, *, encoding: str = "latin1", unified_space: bool = False):
        yield RawObject(data.encode(encoding) + b"\x00", unified_space)


def uint_width_for_max(value: int) -> int:
    return math.ceil(math.log2(value) / 8)


class GeneratorWrapper:
    def __init__(self, gen):
        self.gen = gen

    def __iter__(self):
        self.value = yield from self.gen


@dataclass(frozen=True)
class ArrayIndexPackResult(PackResult):
    addr_bytes: int

    def __repr__(self) -> str:
        return super().__repr__() + f", {self.addr_bytes} bytes per address"


@dataclass(frozen=True)
class ArrayIndexObject(DataObject):
    array_name: str
    address: List[int]

    def pack(self) -> PackResult:
        addr_bytes = uint_width_for_max(self.address[-1])
        data = bytearray()
        for addr in self.address:
            data += addr.to_bytes(addr_bytes, "little")
        return ArrayIndexPackResult(data, addr_bytes)

    def get_type_name(self) -> str:
        return "array index"


class Packer:
    """
    Class at the center of the DSL used in assets packing scripts.
    The class has various methods to describe objects to be packed in assets.
    Each asset has a name, a group, and a location (internal or flash memory).

    Multiple assets can also be grouped into arrays::

        with packer.array("name", ArrayType.REGULAR):
            packer.image(...)

    There are a few built-in object types: image, sound, font, raw, raw_file.
    The class also supports registration of arbitrary builders for extending functionality
    via the `builder`, `file_builder` and `helper` decorators.
    """
    _objects: List[PackObject]
    _defines: Dict[str, int]
    _group_names: Set[str]
    _array_types: Dict[str, ArrayType]
    _curr_group: List[str]
    _is_in_array: bool
    _location: Location

    # byte value used for padding in regular arrays
    PADDING_BYTE = b"\xff"

    # dimensions in pixel of an app cover image
    APP_COVER_DIMENSIONS = (124, 54)

    PREFIX = "asset_"

    def __init__(self, cover_image: PathLike):
        """Initialize a packer and set the cover image for the app."""
        self._objects = []
        self._defines = {}
        self._group_names = set()
        self._array_types = {}
        self._curr_group = []
        self._is_in_array = False
        self._location = Location.FLASH

        # register builders for built-in object types
        register_builtin_builders(self)
        font_gen.register_builder(self)
        image_gen.register_builder(self)
        sound_gen.register_builder(self)

        # add cover image as first object
        cover_image_path = Path(ASSETS_DIR, cover_image)
        with Image.open(cover_image_path) as img:
            if img.size != Packer.APP_COVER_DIMENSIONS:
                self._error(f"app cover image dimensions must be exactly " +
                            'x'.join(str(d) for d in Packer.APP_COVER_DIMENSIONS))
        self.image(cover_image_path, raw=False, binary=False, opaque=True)

    def _error(self, message, obj: Union[int, PackObject, None] = None) -> NoReturn:
        """Used to report an error when processing a particular object."""
        location = ""
        if isinstance(obj, PackObject):
            location = f" (in object with name '{obj.name}')"
        elif isinstance(obj, int):
            location = f" (in object at index {obj})"
        print(f"ERROR: {message}{location}.")
        exit(1)

    def _warn(self, message: str) -> None:
        print(f"WARNING: {message}.")

    def _check_name(self, name: str) -> str:
        name = name.strip().lower()
        if not name:
            self._error("object name cannot be blank", len(self._objects))
        if not re.match(r"^[a-z\d_]+$", name):
            self._error(f"invalid object name '{name}'", len(self._objects))
        return name

    @property
    def location(self):
        """The location in which to store new objects."""
        return self._location

    @location.setter
    def location(self, value: Location):
        if self._is_in_array:
            self._error("location cannot be changed within array")
        self._location = value

    def _curr_group_name(self) -> str:
        return '_'.join(self._curr_group)

    @contextmanager
    def group(self, name: str, location: Location = None):
        """Context manager used to create a new group.
        These can be nested to create nested groups."""
        if self._is_in_array:
            self._error("arrays cannot contain groups or other arrays")

        name = name.lower()
        if not re.match(r"^[a-z\d_]+$", name):
            self._error(f"group name '{name}' is invalid.")

        self._curr_group.append(name)
        group_name = self._curr_group_name()
        if group_name in self._group_names:
            self._error(f"group '{group_name}' already exists. Groups cannot be split")
        self._group_names.add(group_name)

        if not location:
            location = self.location

        old_location = self.location
        self._location = location
        try:
            yield self
        finally:
            self._location = old_location
            del self._curr_group[-1]

    @contextmanager
    def array(self, name: str, array_type: ArrayType, location: Location = None):
        """Context manager used to create a new array. Arrays cannot be nested and cannot
        contain groups. Ideally, an array should contain objects of the same type, but this is
        not enforced (declarations are made assuming the type of the first object).
        All array types in all locations generate a macro that can be used transparently,
        without knowing how and where the array is stored. The macro result should be cached though.
        Different array types offer a size--performance compromise."""
        if not location:
            location = self.location
        if location == Location.INTERNAL and array_type == ArrayType.INDEXED_ABS_FLASH:
            self._error(f"{ArrayType.INDEXED_ABS_FLASH.name} array type "
                        f"cannot be stored in internal memory.")
        size_before = len(self._objects)
        error = False
        try:
            with self.group(name, location):
                self._is_in_array = True
                self._array_types[self._curr_group_name()] = array_type
                yield
        except:
            # If there's an exception causing the array to be empty, the "empty array"
            # error thrown below will hide that first exception and make debugging difficult.
            error = True
            raise
        finally:
            self._is_in_array = False
            if not error and size_before == len(self._objects):
                self._error(f"array '{name}' is empty")

    def _add_objects(self, name: str, objects: List[DataObject]) -> None:
        group = self._curr_group_name()
        for i, obj in enumerate(objects):
            obj_name = f"{name}_{i}" if len(objects) > 1 else name
            self._objects.append(PackObject(obj_name, group, self.location, obj))

    def builder(self, func: Generator[DataObject, None, Any]):
        """
        Decorator to use on a function to register as an object builder for the packer instance.
        Builders take any number of parameters and keyword parameters and generate a packer method
        with an additional "name" parameter (a required keyword-only parameter).
        The decorated function is expected to yield zero or more DataObjects and may return a
        value that will be returned afterwards by the builder.
        It can also raise a PackError to indicate an error.
        """

        def wrapper(*args, name: Optional[str], **kwargs) -> Any:
            try:
                gen = GeneratorWrapper(func(*args, **kwargs))
                objects = list(gen)
            except PackError as e:
                self._error(e, len(self._objects))
            name = self._check_name(name)
            self._add_objects(name, objects)
            return gen.value

        setattr(self, func.__name__, wrapper)
        return func

    def file_builder(self, func: Generator[DataObject, None, Any]):
        """Same as `builder` decorator but also adds a "filename" argument to the function as
        the first parameter (non-keyword, Path type). Also the name parameter becomes optional
        since it can be deduced automatically from the filename."""

        def wrapper(*args, name: Optional[str] = None, **kwargs) -> Any:
            filename = args[0]
            filename = filename if isinstance(filename, Path) else Path(ASSETS_DIR, filename)
            if not filename.exists():
                self._error(f"file '{filename}' doesn't exist", len(self._objects))
            if name is None:
                # deduce name from filename
                name = filename.stem.lower()
                name = re.sub(r"[-_ .]+", "_", name)

            try:
                gen = GeneratorWrapper(func(filename, *args[1:], **kwargs))
                objects = list(gen)
            except PackError as e:
                self._error(repr(e), len(self._objects))
            name = self._check_name(name)
            self._add_objects(name, objects)
            return gen.value

        setattr(self, func.__name__, wrapper)
        return func

    def helper(self, func: Callable) -> None:
        """Used to create a helper function for a particular type of data object."""
        setattr(self, func.__name__, func)

    def define(self, name: str, value: int, asset_prefix: bool = True) -> None:
        """Add an extra #define macro to generate in assets header file.
        The define name is prefixed with the current group name and may be prefixed with ASSET_"""
        name_lower = name.strip().lower()
        if not re.match(r"^[a-z\d_]+$", name_lower):
            self._error(f"bad define name '{name}'")
        full_name = f"{self._curr_group_name()}_{name_lower}"
        if asset_prefix:
            full_name = Packer.PREFIX + full_name
        self._defines[full_name] = value

    # The following methods are implemented by registered builder (via a decorator),
    # but still declared here to get the types signature. The builder overrides these stubs.
    # ==========================

    def image(self, filename: PathLike, *,
              region: Tuple[int, int, int, int] = None, raw: bool = False, binary: bool = None,
              opaque: bool = None, indexed: bool = None, index_granularity: str = None,
              name: str = None) -> None:
        pass  # implemented by registered builder

    def sound(self, filename: PathLike, *, tempo: int,
              octave_adjust: int = None, merge_midi_tracks: bool = None,
              time_range: slice = None, channels: Set[int] = None, name: str = None) -> None:
        pass  # implemented by registered builder

    def font(self, filename: PathLike, *, glyph_width: int, glyph_height: int,
             extra_line_spacing: int = None, name: str = None) -> None:
        pass  # implemented by registered builder

    def raw(self, data: Iterable[int], *, name: str, unified_space: bool = False) -> None:
        pass  # implemented by registered builder

    def raw_file(self, filename: PathLike, *,
                 unified_space: bool = False, name: str = None) -> None:
        pass  # implemented by registered builder

    def string(self, data: str, *, name: str,
               encoding: str = "latin1", unified_space: bool = False) -> None:
        pass  # implemented by registered builder

    # ==========================

    def pack(self) -> None:
        """Pack all assets"""
        self._pack_assets()
        self._process_regular_arrays()
        self._assign_addresses()
        self._process_flash_indexed_arrays()
        self._print_memory_map()
        self._write_assets_file()

        gen = CodeGenerator(HEADER_FILE, SOURCE_FILE,
                            ArrayType.INDEXED_ABS_FLASH in self._array_types.values())
        self._write_source_objects(gen)
        self._write_source_arrays(gen)
        gen.end()
        if not gen.source_written:
            os.remove(SOURCE_FILE)

    def _iterate_arrays(self) -> Generator[List[PackObject], None, None]:
        array_start = -1
        curr_array = ""
        for i, obj in enumerate(self._objects):
            if obj.group != curr_array:
                curr_array = obj.group
                if array_start != -1:
                    yield self._objects[array_start:i]
                    array_start = -1
                if curr_array in self._array_types:
                    array_start = i
        if array_start != -1:
            yield self._objects[array_start:]

    def _pack_assets(self) -> None:
        """Pack all objects and show progress."""
        n = len(self._objects)
        progress = print_progress_bar("Packing objects", " objects", lambda v: v)
        for i, obj in enumerate(self._objects):
            progress(i, n)
            try:
                obj.result = obj.data.pack()
            except PackError as e:
                self._error(e, obj)
        progress(n, n)
        print()

    def _process_regular_arrays(self) -> None:
        """Add padding after elements of regular arrays."""
        for objects in self._iterate_arrays():
            arr_type = self._array_types[objects[0].group]
            if arr_type == ArrayType.REGULAR:
                # regular array: pad all elements so that they are spaced equally
                max_size = max(len(obj.result.data) for obj in objects)
                for obj in objects:
                    obj.pad_after = max_size - len(obj.result.data)

    def _assign_addresses(self) -> None:
        """Assign addresses to all objects in flash memory."""
        addr = 0
        for obj in self._objects:
            if obj.location != Location.FLASH:
                continue
            obj.address = addr
            addr += len(obj.result.data) + obj.pad_after

    def _process_flash_indexed_arrays(self) -> None:
        """Create index objects for arrays with an index in flash."""
        for objects in self._iterate_arrays():
            arr_name = objects[0].group
            arr_type = self._array_types[arr_name]
            if arr_type == ArrayType.INDEXED_ABS_FLASH:
                # create the index object at place it last in objects list.
                # also pack it and assign an address since these steps must have been done already.
                index = ArrayIndexObject(arr_name, [obj.address for obj in objects])
                obj = PackObject(f"{arr_name}.index", "", Location.FLASH, index)
                obj.result = index.pack()
                obj.address = self._objects[-1].address + len(self._objects[-1].result.data)
                self._objects.append(obj)

    def _print_memory_map(self) -> None:
        """Print a list of result for packed objects by location."""

        def do_print(objects: List[PackObject], section: str, show_address: bool) -> None:
            section_size = sum(len(obj.result.data) for obj in objects)
            addr_width = max(3, len(f"{section_size:x}")) if show_address else 0
            if not objects:
                return
            print(f"{section.upper()} ({readable_size(section_size)})")
            print("=======================")
            for obj in objects:
                if show_address:
                    print(f"0x{f'{obj.address:x}'.rjust(addr_width, '0')}: ", end="")
                name = obj.name
                if obj.group:
                    name = f"{obj.group}/" + name
                res_str = repr(obj.result).replace("\n", "\n" + " " * (addr_width + 4))
                print(f"{name}, {obj.data.get_type_name()}, {res_str}")
            print()

        # group objects by location
        by_location = {loc: [] for loc in Location}
        for obj in self._objects:
            by_location[obj.location].append(obj)

        do_print(by_location[Location.INTERNAL], "internal", False)
        do_print(by_location[Location.FLASH], "flash", True)

    def _write_assets_file(self) -> None:
        """Place objects contiguously and write the result to a file."""
        print("Writing assets file... ", end="")
        try:
            with open(ASSETS_FILE, "wb") as file:
                for obj in self._objects:
                    if obj.location == Location.FLASH:
                        file.write(obj.result.data)
                        for i in range(obj.pad_after):
                            file.write(Packer.PADDING_BYTE)
            print("DONE")
        except IOError as e:
            self._error(f"could not write assets file: {e}")

    def _write_source_objects(self, gen: CodeGenerator) -> None:
        """Write header and source code for all objects not in arrays, including defines."""
        if self._defines:
            for name, value in self._defines.items():
                gen.add_define(name.upper(), value)
            gen.add_separator()

        last_group = None
        for i, obj in enumerate(self._objects):
            if obj.group in self._array_types or isinstance(obj.data, ArrayIndexObject):
                continue
            if i != 0 and obj.group != last_group:
                gen.add_separator()
            last_group = obj.group

            name = obj.name
            if obj.group:
                name = f"{obj.group}_{name}"
            name = f"{Packer.PREFIX}{name}"
            location = obj.location if obj.data.is_in_unified_data_space() else None
            if obj.location == Location.INTERNAL:
                # generate array containing internal data
                if obj.data.is_in_unified_data_space():
                    data_name = f"{name}_data".upper()
                    gen.add_array(data_name, 1, obj.result.data)
                    gen.add_define(name, data_name, location=location)
                else:
                    gen.add_array(name, 1, obj.result.data)
            else:
                # generate define for the object address
                gen.add_define(name, obj.address, True, location)

        gen.add_separator()

    def _write_source_arrays(self, gen: CodeGenerator) -> None:
        for objects in self._iterate_arrays():
            first = objects[0]

            name = f"{Packer.PREFIX}{first.group}"
            name_u = name.upper()
            arr_type = self._array_types[first.group]
            location = first.location if first.data.is_in_unified_data_space() else None

            gen.add_define(f"{name}_size", len(objects))

            if first.location == Location.INTERNAL:
                # Create the array data. All elements are packed within a single array.
                # Also assign their address within the array data to the objects.
                data = bytearray()
                for obj in objects:
                    obj.address = len(data)
                    data += obj.result.data
                    for i in range(obj.pad_after):
                        data += Packer.PADDING_BYTE
                gen.add_array(f"{name}", 1, data)

                if arr_type == ArrayType.REGULAR:
                    # address + offset
                    gen.add_define(f"{name}_offset", len(first.result.data) + first.pad_after)
                    gen.add_macro(name, ["n"], f"{name_u}[(n) * {name_u}_OFFSET]", location)

                elif arr_type == ArrayType.INDEXED_ABS:
                    # array of absolute positions within the data
                    addr = [obj.address for obj in objects]
                    gen.add_ptr_array(f"{name}_addr", 1,
                                      [f"&{name_u}[{obj.address}]" for obj in objects])
                    gen.add_macro(f"{name}", ["n"], f"{name_u}_ADDR[n]", location)

                elif arr_type == ArrayType.INDEXED_REL:
                    # address + array of relative offsets within the data
                    offset = [(obj.address - first.address) for obj in objects]
                    type_width = uint_width_for_max(max(offset))
                    gen.add_array(f"{name}_offset", type_width, offset)
                    gen.add_macro(f"{name}", ["n"], f"{name_u}_ADDR[{name_u}_OFFSET[n]]", location)

            else:
                if arr_type == ArrayType.REGULAR:
                    # address + offset
                    gen.add_define(f"{name}_addr", first.address, is_hex=True)
                    gen.add_define(f"{name}_offset", len(first.result.data) + first.pad_after)
                    gen.add_macro(name, ["n"], f"{name_u}_ADDR + (n) * {name_u}_OFFSET", location)

                elif arr_type == ArrayType.INDEXED_ABS:
                    # array of absolute addresses
                    addr = [obj.address for obj in objects]
                    type_width = uint_width_for_max(addr[-1])
                    gen.add_array(name, type_width, addr)
                    gen.add_macro(f"{name}", ["n"], f"{name_u}[n]", location)

                elif arr_type == ArrayType.INDEXED_REL:
                    # address + array of relative offsets within the data
                    offset = [(obj.address - first.address) for obj in objects]
                    type_width = uint_width_for_max(max(offset))
                    gen.add_define(f"{name}_addr", first.address, is_hex=True)
                    gen.add_array(f"{name}_offset", type_width, offset)
                    gen.add_macro(f"{name}", ["n"], f"{name_u}_ADDR + {name_u}_OFFSET[n]", location)

                elif arr_type == ArrayType.INDEXED_ABS_FLASH:
                    # address of index + bytes per address
                    index = next((obj for obj in self._objects
                                  if isinstance(obj.data, ArrayIndexObject) and
                                  obj.data.array_name == first.group))
                    addr_size = index.result.addr_bytes
                    assert isinstance(index.result, ArrayIndexPackResult)
                    gen.add_define(f"{name}_index", index.address, is_hex=True)
                    gen.add_define(f"{name}_addr_size", addr_size)
                    gen.add_macro(f"{name}", ["n"],
                                  f"({{uint{addr_size * 8}_t _a; flash_read({name_u}_INDEX + "
                                  f"(n) * {name_u}_ADDR_SIZE, {addr_size}, &_a); _a;}})", location)

            gen.add_separator()
