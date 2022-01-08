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

import abc
import math
import re
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional, Union, NoReturn, Tuple, Type, Dict, Set

import font_gen
import image_gen
import sound_gen
from font_gen import FontData
from image_gen import ImageData, Rect, ImageEncoder, IndexGranularity
from sound_gen import SoundData


class PackError(Exception):
    pass


@dataclass
class PackResult:
    data: bytes = field(repr=False)

    def __repr__(self) -> str:
        return readable_size(len(self.data))


@dataclass
class DataObject(abc.ABC):
    name: str
    group: Optional[str]

    def pack(self, addr: int) -> PackResult:
        raise NotImplementedError


@dataclass
class FileObject(DataObject, abc.ABC):
    file: Path


@dataclass
class ImagePackResult(PackResult):
    image_data: ImageData

    def __repr__(self) -> str:
        d = self.image_data
        s = super().__repr__()
        s += f", {d.width}x{d.height} px, "
        s += ("1-bit" if d.binary else "4-bit") + ", "
        if d.indexed:
            s += f"indexed every {d.index.granularity} rows, " \
                 f"max {max(d.index.entries[1:])} bytes between rows"
        else:
            s += f"not indexed"
        return s


@dataclass
class ImageObject(FileObject):
    region: Optional[Rect]
    indexed: bool
    index_granularity: str
    force_binary: bool

    def pack(self, addr: int) -> ImagePackResult:
        try:
            index_granularity = IndexGranularity.from_str(self.index_granularity)
        except ValueError as e:
            raise PackError(f"invalid index granularity: {e}")
        config = image_gen.Config(self.file, "", self.region, self.indexed,
                                  index_granularity, self.force_binary, False)

        try:
            image_data = image_gen.create_image_data(config)
            data = image_data.encode()
        except image_gen.EncodeError as e:
            raise PackError(f"encoding error: {e}")
        return ImagePackResult(data, image_data)


@dataclass
class FontPackResult(PackResult):
    font_data: FontData

    def __repr__(self) -> str:
        d = self.font_data
        glyph_count = len(d.glyphs)
        s = super().__repr__()
        s += f", {glyph_count} glyphs, {d.bytes_per_glyph} bytes per glyph,\n" \
             f"{d.offset_bits} offset bits (max offset is {d.max_offset} px), encode ranges "
        total = 0
        for r in FontData.RANGES:
            if total + len(r) > glyph_count:
                s += f"{r.start:#02x}-{r.start + (glyph_count - total):#02x}, "
                break
            else:
                s += f"{r.start:#02x}-{r.stop - 1:#02x}, "
                total += len(r)
        return s[:-2]


@dataclass
class FontObject(FileObject):
    glyph_width: int
    glyph_height: int
    line_spacing: int

    def pack(self, addr: int) -> PackResult:
        config = font_gen.Config(self.file, "", self.glyph_width, self.glyph_height,
                                 self.line_spacing, False)
        try:
            font_data = font_gen.create_font_data(config)
            data = font_data.encode()
        except font_gen.EncodeError as e:
            raise PackError(f"encoding error: {e}")
        return FontPackResult(data, font_data)


@dataclass
class SoundPackResult(PackResult):
    sound_data: SoundData

    def __repr__(self) -> str:
        d = self.sound_data
        s = super().__repr__()
        s += f", on channels {', '.join((str(t.channel) for t in self.sound_data.tracks))}"
        return s


@dataclass
class SoundObject(FileObject):
    tempo_bpm: int
    octave_adjust: int
    merge_midi_tracks: bool
    time_range: Optional[slice]
    channels: Set[int]

    def pack(self, addr: int) -> PackResult:
        tempo_us = round(sound_gen.bpm_to_beat_us(self.tempo_bpm))
        config = sound_gen.Config(self.file, "", tempo_us, self.octave_adjust,
                                  self.merge_midi_tracks, self.time_range, self.channels, False)
        try:
            sound_data = sound_gen.create_sound_data(config)
            data = sound_data.encode()
        except sound_gen.EncodeError as e:
            raise PackError(f"encoding error: {e}")
        return SoundPackResult(data, sound_data)


@dataclass
class RawDataObject(DataObject):
    data: bytes

    def pack(self, addr: int) -> PackResult:
        return PackResult(self.data)


@dataclass
class RawFileObject(FileObject):
    def pack(self, addr: int) -> PackResult:
        try:
            with open(self.file, "rb") as file:
                data = file.read()
        except IOError as e:
            raise PackError(f"could not read raw file: {e}")
        return PackResult(data)


@dataclass
class ArrayIndexObject(RawDataObject):
    addr_bytes: int


@dataclass
class SpaceObject(DataObject):
    value: int
    size: int

    def pack(self, addr: int) -> PackResult:
        data = bytearray(self.size)
        for i in range(self.size):
            data[i] = self.value
        return PackResult(data)


@dataclass
class PadObject(DataObject):
    value: int
    bound: int

    def pack(self, addr: int) -> PackResult:
        padding = addr - addr % self.bound
        data = bytearray()
        if padding != self.bound:
            for i in range(self.bound):
                data.append(self.value)
        return PackResult(data)


TYPE_NAMES: Dict[Type[DataObject], str] = {
    ImageObject: "image",
    FontObject: "font",
    SoundObject: "sound",
    RawFileObject: "raw file",
    RawDataObject: "raw data",
    ArrayIndexObject: "array index",
    SpaceObject: "(space)",
    PadObject: "(pad)",
}


class ArrayType(Enum):
    # no array, treat as separate objects (default)
    NONE = 0
    # array with regularly spaced elements (address + single offset)
    # if elements have different sizes, padding is added between them.
    REGULAR = 1
    # array with unevenly spaced elements (multiple addresses stored in program memory)
    INDEXED_ABS = 2
    # array with unevenly spaced elements (address + multiple offsets stored in program memory)
    INDEXED_REL = 3
    # array with unevenly spaced elements (index address + multiple addresses stored in flash)
    INDEXED_ABS_FLASH = 4


class CodeGenerator:
    """Basic C code generator for generating assets header and source."""

    @dataclass
    class Define:
        name: str
        value: int
        is_hex: bool

    @dataclass
    class Array:
        name: str
        type_width: int
        values: List[int]

    Separator = "separator"

    _elements: List[Union[Define, Array, Separator]]

    def __init__(self):
        self._elements = []

    def add_define(self, name: str, value: int, is_hex: bool = False) -> None:
        self._elements.append(CodeGenerator.Define(name, value, is_hex))

    def add_array(self, name: str, type_width: int, values: List[int]) -> None:
        self._elements.append(CodeGenerator.Array(name, type_width, values))

    def add_separator(self):
        self._elements.append(CodeGenerator.Separator)

    def write_header(self, out: Path):
        lines = [
            "// file auto-generated by assets packer, do not modify directly",
            "#ifndef ASSETS_H",
            "#define ASSETS_H",
            "",
        ]
        if any((isinstance(e, CodeGenerator.Array) for e in self._elements)):
            lines += ["#include <sys/defs.h>", ""]

        max_hex_value = max((e.value for e in self._elements
                             if isinstance(e, CodeGenerator.Define) and e.is_hex))
        hex_value_width = len(f"{max_hex_value:x}")
        name_width = max((len(e.name) for e in self._elements if e != CodeGenerator.Separator))
        for e in self._elements:
            if isinstance(e, CodeGenerator.Define):
                value: str
                if e.is_hex:
                    value = f"0x{f'{e.value:x}'.rjust(hex_value_width, '0')}"
                else:
                    value = f"{e.value}"
                if e.value > 0xffff:
                    value += "L"
                lines.append(f"#define {e.name.ljust(name_width)} {value}")
            elif isinstance(e, CodeGenerator.Array):
                lines.append(f"extern const uint{e.type_width}_t {e.name}[];")
            else:
                lines.append("")

        lines += [
            "",
            "#endif"
        ]
        with open(out, "w") as file:
            for line in lines:
                file.write(line)
                file.write("\n")

    def write_source(self, out: Path):
        lines = [
            "// file auto-generated by assets packer, do not modify directly",
            '#include "assets.h"',
            ""
        ]
        arrays_count = 0
        for e in self._elements:
            if isinstance(e, CodeGenerator.Array):
                lines.append(f"const uint{e.type_width}_t {e.name}[] = {{")
                line = ""
                max_value_size = len(f"{max(e.values):x}")
                for i, value in enumerate(e.values):
                    if i % 8 == 0:
                        line = "        "
                    line += f"0x{f'{value:x}'.rjust(max_value_size, '0')}, "
                    if i == len(e.values) - 1 or i % 8 == 7:
                        lines.append(line)
                lines.append("};")
                lines.append("")
                arrays_count += 1

        if arrays_count > 0:
            with open(out, "w") as file:
                for line in lines:
                    file.write(line)
                    file.write("\n")


class Packer:
    """Class used to create a flash image containing all assets, as well as a header
    with the memory map. Supports raw data, images, fonts, and sound."""
    objects: List[DataObject]
    directory: str
    padding_byte: int
    padding: int
    default_indexed: bool
    default_index_granularity: str
    offset: int

    _packed: bool
    _mem_map: Optional[List[int]]
    _array_types: Dict[str, ArrayType]

    # maximum size of resulting data file (stored in flash)
    MAX_SIZE = 1048576

    def __init__(self, *, assets_directory: str = "", offset: int = 0, padding_byte: int = 0xff,
                 padding: int = 0, default_indexed: bool = True,
                 default_index_granularity: str = repr(ImageEncoder.DEFAULT_INDEX_GRANULARITY)):
        self.objects = []
        self.directory = assets_directory
        self.offset = offset
        self.padding_byte = padding_byte
        self.padding = padding
        self.default_indexed = default_indexed
        self.default_index_granularity = default_index_granularity
        self._packed = False
        self._mem_map = None
        self._array_types = {}

    def _error(self, message: str, obj: Union[int, DataObject, None] = None) -> NoReturn:
        """Used to report an error when processing a particular object."""
        location = ""
        if isinstance(obj, FileObject):
            location = f" (in object from '{obj.file}')"
        elif isinstance(obj, DataObject):
            location = f" (in object with name '{obj.name}')"
        elif isinstance(obj, int):
            location = f" (in object #{obj})"
        print(f"ERROR: {message}{location}.")
        exit(1)

    def _check_not_packed(self) -> None:
        if self._packed:
            self._error("cannot perform operation, already packed")

    def _check_filename(self, filename: str) -> Path:
        """Check if file exists and is valid."""
        path = Path(self.directory, filename)
        if not path.exists() or not path.is_file():
            self._error("invalid file", len(self.objects))
        return path

    def _name_or_default(self, name: Optional[str], filename: str) -> str:
        """Validate object name, use filename if none."""
        if name is None:
            name = Path(filename).stem
        else:
            name = name.strip()
            if not name:
                self._error("object name cannot be blank", len(self.objects))
        if not re.match(r"^[a-zA-Z0-9\-_ .]+$", name):
            self._error(f"invalid object name '{name}'", len(self.objects))
        name = re.sub(r"[-_ .]+", "_", name)
        return name

    def _check_group(self, group: str) -> str:
        """Validate group name."""
        group = group.strip()
        if not group:
            self._error("object group cannot be blank", len(self.objects))
        if not re.match(r"^[a-zA-Z0-9\-_ .]+$", group):
            self._error(f"invalid object name '{group}'", len(self.objects))
        name = re.sub(r"[-_ .]+", "_", group)
        return group

    def set_array_type(self, group: str, array_type: ArrayType) -> None:
        """Used to indicate that a group should create an array of the specified type."""
        self._array_types[group] = array_type

    def font(self, filename: str, group: str = "fnt", *, glyph_width: int, glyph_height: int,
             extra_line_spacing: int = 1, name: Optional[str] = None) -> None:
        """Add a font file to the data to pack. See font_gen.py for more info on parameters."""
        self._check_not_packed()
        self.objects.append(FontObject(
            self._name_or_default(name, filename),
            self._check_group(group),
            self._check_filename(filename),
            glyph_width,
            glyph_height,
            glyph_height + extra_line_spacing
        ))

    def image(self, filename: str, group: str = "img", *,
              region: Optional[Tuple[int, int, int, int]] = None, indexed: bool = None,
              index_granularity: Optional[str] = None, force_binary: bool = False,
              name: Optional[str] = None) -> None:
        """Add an image file to the data to pack. See image_gen.py for more info on parameters."""
        self._check_not_packed()
        self.objects.append(ImageObject(
            self._name_or_default(name, filename),
            self._check_group(group),
            self._check_filename(filename),
            None if region is None else Rect(*region),
            self.default_indexed if indexed is None else indexed,
            self.default_index_granularity if index_granularity is None else index_granularity,
            force_binary
        ))

    def sound(self, filename: str, group: str = "sound", *, tempo: int, octave_adjust: int = 0,
              merge_midi_tracks: bool = False, time_range: Optional[slice] = None,
              channels: Optional[Set[int]] = None, name: Optional[str] = None) -> None:
        """Add a sound file to the data to pack. See sound_gen.py for more info on parameters."""
        self._check_not_packed()
        self.objects.append(SoundObject(
            self._name_or_default(name, filename),
            self._check_group(group),
            self._check_filename(filename),
            tempo,
            octave_adjust,
            merge_midi_tracks,
            time_range,
            channels if channels is not None else set(range(SoundData.CHANNELS_COUNT))
        ))

    def raw(self, data: Union[str, bytes], group: str = "raw", *,
            name: Optional[str] = None) -> None:
        """Add a raw data file or a raw data array to the data to pack.
        The name parameter is required if a data array is used."""
        self._check_not_packed()
        group = self._check_group(group)
        if isinstance(data, str):
            name = self._name_or_default(name, data)
            filename = self._check_filename(data)
            self.objects.append(RawFileObject(name, group, filename))
        else:
            if name is None:
                self._error("raw data object must be given a name", len(self.objects))
            name = self._name_or_default(name, "")
            self.objects.append(RawDataObject(name, group, data))

    def space(self, size: int, *, value: int = 0xff) -> None:
        """Add a space filled with the same byte value to the data to pack."""
        self._check_not_packed()
        self.objects.append(SpaceObject("", None, value, size))

    def pad(self, bound: int) -> None:
        """Pad the data added previously to an address boundary (in bytes)."""
        self._check_not_packed()
        self.objects.append(PadObject("", None, self.padding_byte, bound))

    def _print_pack_results(self, total_size: int, pack_results: List[PackResult]) -> None:
        """Print pack results and object addresses, as well as total size."""
        addr_width = len(f"{total_size:x}")
        for i in range(len(self.objects)):
            addr = self._mem_map[i]
            obj = self.objects[i]
            res = pack_results[i]
            type_name = TYPE_NAMES[type(obj)]
            print(f"0x{f'{addr:x}'.rjust(addr_width, '0')}: ", end="")
            if obj.name or obj.group:
                obj_id = ""
                if obj.group:
                    obj_id += f"{obj.group}/"
                if obj.name:
                    obj_id += obj.name
                print(f"{obj_id}, ", end="")
            res_str = repr(res).replace("\n", "\n" + " " * (addr_width + 4))
            print(f"{type_name}, {res_str}")
        print(f"TOTAL SIZE: {readable_size(total_size)}")

        print()
        if self.offset + total_size > Packer.MAX_SIZE:
            print(f"WARNING: end of data exceeds flash capacity ("
                  f"max {readable_size(Packer.MAX_SIZE)}, "
                  f"got {readable_size(total_size)} + {self.offset:#x} offset)")

    ArraysData = Dict[str, Tuple[List[PackResult], int]]

    def _encode_array_objects(self) -> ArraysData:
        """The objects of regular arrays must be encoded before to know the maximum element size and
        do the alignment. It's not necessary for other array types but having everything the same
        simplifies the process."""
        arrays_data: Packer.ArraysData = {}
        for group, array_type in self._array_types.items():
            if array_type == ArrayType.NONE:
                continue

            first_idx = next((i for i in range(len(self.objects))
                              if self.objects[i].group == group), None)
            array_size = sum((1 for obj in self.objects if obj.group == group))
            pack_results = []
            max_size = 0

            for i in range(first_idx, first_idx + array_size):
                obj = self.objects[i]
                if obj.group != group:
                    self._error("objects in array group must be contiguous")

                try:
                    res = obj.pack(0)  # not padding, address doesn't matter
                except PackError as e:
                    self._error(repr(e), obj)
                pack_results.append(res)

                if array_type == ArrayType.REGULAR:
                    # for regular array, get maximum array element size, including padding
                    size = len(res.data)
                    if size % self.padding != 0:
                        size += self.padding - size % self.padding
                    if size > max_size:
                        max_size = size

            arrays_data[group] = (pack_results, max_size)
        return arrays_data

    def _pack_objects(self, data: bytearray, arrays_data: ArraysData) -> List[PackResult]:
        """Pack all objects and save the pack results. Generate the memory map.
        Also encodes all objects that weren't yet (if not part of array)."""
        self._mem_map = []
        pack_results = []
        for i, obj in enumerate(self.objects):
            self._mem_map.append(len(data) + self.offset)

            if obj.group in arrays_data:
                # object is in array, already encoded previously
                array_type = self._array_types[obj.group]
                res = arrays_data[obj.group][0][0]

                data += res.data
                pack_results.append(res)
                del arrays_data[obj.group][0][0]

                if array_type == ArrayType.REGULAR:
                    # add padding to have equally spaced elements
                    for j in range(arrays_data[obj.group][1] - len(res.data)):
                        data.append(self.padding_byte)
                    continue
                elif array_type == ArrayType.INDEXED_ABS_FLASH and not arrays_data[obj.group][0]:
                    # end of array, create array index in flash
                    # we can use 1, 2 or 3 bytes per address.
                    addr_bytes = int(math.log2(self._mem_map[-1]) // 8 + 1)
                    index_data = bytearray()
                    for addr in self._mem_map[-12:]:
                        index_data += addr.to_bytes(addr_bytes, "little")
                    self.objects.append(ArrayIndexObject(obj.group, ".index",
                                                         index_data, addr_bytes))
            else:
                # encode object
                try:
                    res = obj.pack(len(data))
                    pack_results.append(res)
                except PackError as e:
                    self._error(e.args[0], obj)
                data += res.data

            # add default padding
            padding = (self.padding - len(data) % self.padding)
            if padding != self.padding:
                for j in range(padding):
                    data.append(self.padding_byte)
        return pack_results

    def pack(self, output_filename: str) -> None:
        """Pack all objects added and output a binary file."""
        self._check_not_packed()
        if not self.objects:
            self._error("no objects to pack")

        print("Packing in progress...")

        arrays_data = self._encode_array_objects()
        data = bytearray()
        pack_results = self._pack_objects(data, arrays_data)

        print("DONE\n\nMemory map:")
        self._print_pack_results(len(data), pack_results)

        try:
            with open(output_filename, "wb") as file:
                file.write(data)
        except IOError as e:
            self._error(f"could not write output file: {e}")

    def _create_header_array_definition(self, prefix: str,
                                        array_type: ArrayType, gen: CodeGenerator,
                                        objects: List[Tuple[int, DataObject]]) -> None:
        """Create definition and source for an array containing a list of objects."""
        if len(objects) == 1:
            self._error("cannot create array with single object", objects[0][1])

        group = objects[0][1].group
        name = f"{prefix.upper()}_{group.upper()}"
        elements_addr = [self._mem_map[i] for i, _ in objects]

        if array_type == ArrayType.REGULAR:
            gen.add_define(name, elements_addr[0], is_hex=True)
            gen.add_define(f"{name}_OFFSET", elements_addr[1] - elements_addr[0])
            gen.add_define(f"{name}_SIZE", len(objects))

        elif array_type == ArrayType.INDEXED_ABS or array_type == ArrayType.INDEXED_REL:
            array_name = name
            if array_type == ArrayType.INDEXED_REL:
                start_addr = elements_addr[0]
                gen.add_define(name, start_addr, is_hex=True)
                elements_addr = [(addr - start_addr) for addr in elements_addr]
                array_name += "_OFFSET"

            gen.add_define(f"{name}_SIZE", len(objects))

            type_width = int((math.log2(elements_addr[-1]) // 8 + 1) * 8)
            gen.add_array(array_name, type_width, elements_addr)

        elif array_type == ArrayType.INDEXED_ABS_FLASH:
            # find index object and put its address and the number of bytes per flash address
            iidx, iobj = next(((i, obj) for i, obj in enumerate(self.objects)
                               if isinstance(obj, ArrayIndexObject) and obj.name == group))
            index_addr = self._mem_map[iidx]
            gen.add_define(name, index_addr, is_hex=True)
            gen.add_define(f"{name}_SIZE", len(objects))
            gen.add_define(f"{name}_ADDR_SIZE", iobj.addr_bytes)

    def create_header(self, output_filename: str, output_filename_src: Optional[str] = None, *,
                      prefix: str = "asset") -> None:
        """Create header file with defines for the memory map. Also creates a source file
        if there are arrays with absolute or relative indexing (and not in flash)."""
        if self._mem_map is None:
            self._error("must pack before creating memory map header")
        if not prefix:
            self._error("header prefix cannot be blank")
        output_path_h = Path(output_filename)
        if output_path_h.suffix != ".h":
            self._error("header file must have .h extension")

        output_path_c = Path(output_filename_src if output_filename_src else
                             output_path_h.with_suffix(".c"))
        if output_path_c.suffix != ".c":
            self._error("source file must have .c extension")

        gen = CodeGenerator()

        objects_by_group: Dict[str, List[Tuple[int, DataObject]]] = {}
        for i, obj in enumerate(self.objects):
            if obj.group not in objects_by_group:
                objects_by_group[obj.group] = []
            objects_by_group[obj.group].append((i, obj))

        all_names = set()
        for group, objects in objects_by_group.items():
            if group is None or group.startswith("."):
                continue  # space object
            gen.add_separator()
            if self._array_types.get(group, ArrayType.NONE) != ArrayType.NONE:
                self._create_header_array_definition(prefix, self._array_types[group], gen, objects)
            else:
                for i, obj in objects:
                    name = f"{prefix.upper()}_{group.upper()}_{obj.name.upper()}"
                    if name in all_names:
                        self._error(f"duplicate name in header '{name}'", obj)
                    all_names.add(name)
                    gen.add_define(name, self._mem_map[i], is_hex=True)

        try:
            gen.write_header(output_path_h)
            gen.write_source(output_path_c)
        except IOError as e:
            self._error(f"could not write header file: {e}")


def readable_size(size: int) -> str:
    """Print size in human readable format, with units."""
    if size < 1024:
        return f"{size} B"
    size /= 1024
    for unit in ["k", "M", "G", "T"]:
        if abs(size) < 1024:
            return f"{size:.2f} {unit}B"
        size /= 1024
    raise ValueError()
