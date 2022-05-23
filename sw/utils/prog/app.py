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
from datetime import date
from typing import List, Optional
import crc

from prog.comm import ProgError
from prog.eeprom import EEPROM_SIZE
from prog.flash import FLASH_SIZE
from prog.memory import MemoryDriver, MemoryManager
from utils import *

APP_INDEX_SIZE = 32

FLASH_INDEX_START = 32
FLASH_DATA_START = 2080
FLASH_ENTRY_SIZE = 64

EEPROM_INDEX_START = 8
EEPROM_DATA_START = 448
EEPROM_ENTRY_SIZE = 5

APP_ID_NONE = 0
APP_ID_SYSTEM = 0xff

CODE_PAGE_SIZE = 256


@dataclass
class DataLocation:
    address: int
    size: int


@dataclass
class App:
    app_id: int
    crc_app: int
    crc_code: int
    app_version: int
    boot_version: int
    code_size: int
    page_height: int
    flash_location: DataLocation
    eeprom_location: DataLocation
    build_date: date
    name: str
    author: str

    GC_SIGNATURE = b"gc"

    def encode(self) -> bytes:
        writer = DataWriter()
        writer.write(self.app_id, 1)
        writer.write(self.crc_app, 2)
        writer.write(self.crc_code, 2)
        writer.write(self.app_version, 2)
        writer.write(self.boot_version, 2)
        writer.write(self.code_size, 2)
        writer.write(self.page_height, 1)
        writer.write(self.eeprom_location.address, 2)
        writer.write(self.eeprom_location.size, 2)
        writer.write(self.flash_location.address, 3)
        writer.data += bytearray(8)
        writer.write(self.flash_location.size, 3)
        writer.write((self.build_date.year - 2020) << 9 |
                     self.build_date.month << 5 | self.build_date.day, 2)
        writer.data += self.name.encode("ascii")
        writer.data += bytearray(48 - len(writer.data))
        writer.data += self.author.encode("ascii") + b"\x00"
        writer.data += bytearray(64 - len(writer.data))
        return writer.data

    @staticmethod
    def decode(data: bytes) -> "App":
        reader = DataReader(data)
        app_id = reader.read(1)
        crc_image = reader.read(2)
        crc_code = reader.read(2)
        app_version = reader.read(2)
        boot_version = reader.read(2)
        code_size = reader.read(2)
        page_height = reader.read(1)
        eeprom_start = reader.read(2)
        eeprom_size = reader.read(2)
        flash_start = reader.read(3)
        reader.pos += 8
        total_size = reader.read(3)
        build_date_raw = reader.read(2)
        try:
            build_date = date((build_date_raw >> 9) + 2020,
                              (build_date_raw >> 5) & 0xf, build_date_raw & 0x1f)
        except ValueError:
            # uninitialized
            build_date = date.today()
        name = reader.data[32:48].decode("ascii").strip('\x00')
        author = reader.data[48:64].decode("ascii").strip('\x00')
        return App(app_id, crc_image, crc_code, app_version, boot_version, code_size, page_height,
                   DataLocation(flash_start, total_size), DataLocation(eeprom_start, eeprom_size),
                   build_date, name, author)


@dataclass
class AppData:
    app_id: int
    location: DataLocation

    def encode(self) -> bytes:
        writer = DataWriter()
        writer.write(self.app_id, 1)
        writer.write(self.location.address, 2)
        writer.write(self.location.size, 2)
        return writer.data

    @staticmethod
    def decode(data: bytes) -> "AppData":
        reader = DataReader(data)
        app_id = reader.read(1)
        address = reader.read(2)
        size = reader.read(2)
        return AppData(app_id, DataLocation(address, size))


class AppManager:
    eeprom: MemoryDriver
    flash: MemoryDriver
    confirm: bool
    boot_version: int

    flash_index: List[App]
    eeprom_index: List[AppData]

    def __init__(self, eeprom: MemoryDriver, flash: MemoryDriver, confirm: bool = False):
        self.eeprom = eeprom
        self.flash = flash
        self.confirm = confirm
        self.boot_version = 0
        self.flash_index = []
        self.eeprom_index = []

    def _check_initialized(self) -> None:
        """Check signatures in flash and EEPROM to make sure they were initialized."""
        flash_signature = self.flash.read(0, 2, no_progress)
        eeprom_signature = self.eeprom.read(0, 2, no_progress)
        if flash_signature != App.GC_SIGNATURE or eeprom_signature != App.GC_SIGNATURE:
            raise ProgError("device not initialized, use the 'init' to initialize it")

    def _read_index(self) -> None:
        if self.flash_index and self.eeprom_index:
            return

        self._check_initialized()
        flash_index = self.flash.read(FLASH_INDEX_START, APP_INDEX_SIZE * FLASH_ENTRY_SIZE,
                                      print_progress_bar("Reading flash app index "))
        eeprom_index = self.eeprom.read(EEPROM_INDEX_START, APP_INDEX_SIZE * EEPROM_ENTRY_SIZE,
                                        print_progress_bar("Reading EEPROM app index"))
        print()
        self.flash_index = []
        self.eeprom_index = []
        flash_pos = 0
        eeprom_pos = 0
        for _ in range(APP_INDEX_SIZE):
            self.flash_index.append(App.decode(
                flash_index[flash_pos:flash_pos + FLASH_ENTRY_SIZE]))
            self.eeprom_index.append(AppData.decode(
                eeprom_index[eeprom_pos:eeprom_pos + EEPROM_ENTRY_SIZE]))
            flash_pos += FLASH_ENTRY_SIZE
            eeprom_pos += EEPROM_ENTRY_SIZE

    @staticmethod
    def _find_app(index: List, app_id: int) -> int:
        """Find an app in the an index."""
        return next((i for i, a in enumerate(index) if a.app_id == app_id), -1)

    @staticmethod
    def _print_app(app: App) -> None:
        print(f"[{app.app_id}] {app.name.title()} v{app.app_version}")
        print(f"  Author: {app.author.title()}")
        print(f"  Build date: {app.build_date.isoformat()}")
        print(f"  Size: {readable_size(app.flash_location.size)} "
              f"({readable_size(app.code_size)} code, "
              f"{readable_size(app.flash_location.size - app.code_size)} data)")
        print(f"  EEPROM size: {readable_size(app.eeprom_location.size)}")
        print(f"  Target bootloader: v{app.boot_version}")

    @staticmethod
    def _warn(message: str) -> None:
        print(f"WARNING: {message}.")

    def _confirm(self, message: str) -> bool:
        if not self.confirm:
            return True
        while True:
            ans = input(f"{message} [Y/n] ").lower()
            if ans == "" or ans == "y":
                print()
                return True
            elif ans == "n":
                print()
                return False

    def initialize(self) -> None:
        """Initialize flash and eeprom by writing signature and zeroing out the region before
        where the data is stored, which notably includes the app index.
        Already initialized memories are left untouched."""

        # check existing signatures
        flash_signature = self.flash.read(0, 2, no_progress)
        eeprom_signature = self.eeprom.read(0, 2, no_progress)

        if flash_signature == App.GC_SIGNATURE and eeprom_signature == App.GC_SIGNATURE:
            print("Device is already initialized, nothing to do.")
            return

        if flash_signature != App.GC_SIGNATURE:
            print("Initializing flash memory...")
            data = bytearray(FLASH_DATA_START)
            data[0:2] = App.GC_SIGNATURE
            flash_writer = MemoryManager(self.flash)
            flash_writer.write(0, data)
            flash_writer.execute()
        else:
            print("Flash memory already initialized.")
        print()

        if eeprom_signature != App.GC_SIGNATURE:
            print("Initializing EEPROM memory...")
            data = bytearray(EEPROM_DATA_START)
            data[0:2] = App.GC_SIGNATURE
            eeprom_writer = MemoryManager(self.eeprom)
            eeprom_writer.write(0, data)
            eeprom_writer.execute()
        else:
            print("EEPROM memory already initialized.")

    @staticmethod
    def _find_write_address(used_space: List[DataLocation],
                            old_location: Optional[DataLocation],
                            new_size: int, device_name: str, device_size: int,
                            device_start: int) -> Optional[int]:
        total_left = device_size - device_start - sum(loc.size for loc in used_space)
        if used_space:
            used_space.sort(key=lambda loc: loc.address)
            used_space.append(DataLocation(device_size, 0))

            if old_location:
                next_loc = next(loc for i, loc in enumerate(used_space)
                                if loc.address > old_location.address)
                if old_location.address + old_location.size <= next_loc.address:
                    # app can be written in place at the same location
                    return old_location.address

            last_end = device_size
            for loc in used_space:
                if loc.address - last_end >= new_size:
                    # app can be written in a space in between two apps, or after the last app
                    return last_end
                last_end = loc.address + loc.size

            if total_left >= new_size:
                # there is enough space if we move other apps around
                return None

        elif total_left >= new_size:
            # first app to be installed, put at device start
            return device_start

        raise ProgError(f"not enough space on device {device_name}, "
                        f"{readable_size(total_left)} left but "
                        f"{readable_size(new_size)} needed")

    def install(self, app_file: PathLike, dont_update: bool, downgrade: bool) -> None:
        """Install or update an app from an app file. If `downgrade` is false, the operation
        will abort if trying to install an app with an older version. Installing an app copies
        the app content to flash and initializes its EEPROM space. There may be existing EEPROM
        space for the app ID, in which case it is reused and extended.

        When writing to flash or EEPROM, the following actions are tried in order:
          1. New apps are written in the first empty space in which they fit.
          2. When updating, if the app fits in existing space, the same address is used.
          3. Otherwise, a continuous empty space large enough if found in memory, if there is one.
          4. If there are no large enough continuous empty space, all other apps are shifted
             to free up space at the end of the device.
          5. If this also fails, then the device is out of memory.
        """
        # read app file
        try:
            with open(app_file, "rb") as file:
                app_data = bytearray(file.read())
        except IOError as e:
            raise ProgError(f"could not read app file '{app_file}': {e}")

        if app_data[0:2] != App.GC_SIGNATURE:
            raise ProgError(f"app file '{app_file}' is not an app image (unknown signature)")
        del app_data[0:2]

        new = App.decode(app_data)
        del app_data[0:FLASH_ENTRY_SIZE]
        app_id = new.app_id

        # check CRCs for good measure...
        crc_calc = crc.CrcCalculator(crc.Crc16.CCITT)
        app_crc = crc_calc.calculate_checksum(app_data)
        code_crc = crc_calc.calculate_checksum(app_data[:new.code_size])
        if app_crc != new.crc_app:
            raise ProgError("App CRC doesn't match the CRC bundled in the app image")
        if code_crc != new.crc_code:
            raise ProgError("Code CRC doesn't match the CRC bundled in the app image")

        if self.boot_version != 0 and new.boot_version != self.boot_version:
            # if boot version is 0, we're in --local mode so this check is ignored.
            AppManager._warn(f"targeted bootloader version (v{new.boot_version}) "
                             f"differs from device bootloader version (v{self.boot_version})")
            print()

        # check if app already exists
        self._read_index()
        flash_index_pos = self._find_app(self.flash_index, app_id)
        flash_update = flash_index_pos != -1
        if flash_update:
            old = self.flash_index[flash_index_pos]
            if dont_update:
                print(f"App with ID {app_id} is already installed, not updating.")
                return
            if new.app_version < old.app_version and not downgrade:
                print(f"New app version ({new.app_version}) is "
                      f"older than current ({old.app_version})")

            # show install info and confirm
            print("Updating existing app:")
            print(f"  Version: v{old.app_version} -> v{new.app_version}")
            print(f"  Name: {old.name.title()} -> {new.name.title()}")
            print(f"  Author: {old.author.title()} -> {new.author.title()}")
            print(f"  Build date: v{old.build_date.isoformat()} -> v{new.build_date.isoformat()}")
            print(f"  Size: {readable_size(old.flash_location.size)} "
                  f"-> {readable_size(new.flash_location.size)}")
            print(f"  EEPROM size: {readable_size(old.eeprom_location.size)} "
                  f"-> {readable_size(new.eeprom_location.size)}")
            print(f"  Target bootloader: v{old.boot_version} -> v{new.boot_version}")
            print()
            if not self._confirm("Update app on device?"):
                return

        else:
            old = None
            print("Installing new app:")
            AppManager._print_app(new)
            print()
            if not self._confirm("Install app on device?"):
                return

        start_time = time.time()

        # find an address in eeprom to write to
        eeprom_used_space = [a.location for a in self.eeprom_index
                             if a.app_id not in [app_id, APP_ID_NONE]]
        eeprom_new_size = new.eeprom_location.size
        eeprom_addr = AppManager._find_write_address(eeprom_used_space,
                                                     old.eeprom_location if old else None,
                                                     eeprom_new_size, "EEPROM",
                                                     EEPROM_SIZE, EEPROM_DATA_START)
        eeprom_index_pos = self._find_app(self.eeprom_index, app_id)
        eeprom_update = eeprom_index_pos != -1
        eeprom_new = AppData(app_id, DataLocation(eeprom_addr, eeprom_new_size))
        if eeprom_new_size == 0:
            eeprom_new.app_id = APP_ID_NONE
        if eeprom_update:
            eeprom_old = self.eeprom_index[eeprom_index_pos]
        else:
            eeprom_old = None
            eeprom_index_pos = self._find_app(self.eeprom_index, APP_ID_NONE)
            if eeprom_index_pos == -1:
                raise ProgError(f"EEPROM index is full (max {APP_INDEX_SIZE} apps)")
        self.eeprom_index[eeprom_index_pos] = eeprom_new

        # find an address in flash to write to
        flash_used_space = [a.flash_location for a in self.flash_index
                            if a.app_id not in [app_id, APP_ID_NONE]]
        flash_addr = AppManager._find_write_address(flash_used_space,
                                                    old.flash_location if old else None,
                                                    new.flash_location.size, "flash",
                                                    FLASH_SIZE, FLASH_DATA_START)
        if not flash_update:
            flash_index_pos = self._find_app(self.flash_index, APP_ID_NONE)
            if flash_index_pos == -1:
                raise ProgError(f"flash index is full (max {APP_INDEX_SIZE} apps)")
        self.flash_index[flash_index_pos] = new

        # write to eeprom (if needed)
        if eeprom_update:
            print("Reallocating EEPROM memory...")
        else:
            print("Initializing EEPROM memory...")
        eeprom_writer = MemoryManager(self.eeprom)
        eeprom_changed = False
        if eeprom_addr is None:
            # pack app data in eeprom
            eeprom_addr = EEPROM_DATA_START
            for i, a in enumerate(self.eeprom_index):
                if a.app_id != app_id:
                    a.location.address = eeprom_addr
                    eeprom_writer.copy(a.location.address, eeprom_addr, a.location.size)
                    eeprom_writer.write(EEPROM_INDEX_START + EEPROM_ENTRY_SIZE * i, a.encode())
                    eeprom_addr += a.location.size

        if eeprom_update:
            if eeprom_addr != eeprom_old.location.address:
                # app data address changed, we must read existing data to copy it elsewhere.
                eeprom_data = self.eeprom.read(eeprom_old.location.address,
                                               eeprom_old.location.size,
                                               print_progress_bar("Reading EEPROM data"))
                eeprom_writer.write(eeprom_addr, eeprom_data)
                eeprom_changed = True
            elif eeprom_old.location.size != eeprom_new_size:
                eeprom_changed = True
        elif eeprom_new_size > 0:
            # initialize eeprom location with zeros
            eeprom_writer.write(eeprom_addr, bytearray(eeprom_new_size))
            eeprom_changed = True

        if eeprom_changed:
            eeprom_writer.write(EEPROM_INDEX_START + EEPROM_ENTRY_SIZE * eeprom_index_pos,
                                eeprom_new.encode())
            eeprom_writer.execute()
        elif eeprom_update:
            print("EEPROM space didn't change with update, nothing to do.")
        else:
            print("App doesn't use any EEPROM space, nothing to do.")
        print()

        # write to flash
        print("Updating flash memory...")
        flash_writer = MemoryManager(self.flash)
        if flash_addr is None:
            # pack apps in flash
            flash_addr = FLASH_DATA_START
            for i, a in enumerate(self.flash_index):
                if a.app_id != app_id:
                    a.flash_location.address = flash_addr
                    flash_writer.copy(a.flash_location.address, flash_addr, a.flash_location.size)
                    flash_writer.write(FLASH_INDEX_START + FLASH_ENTRY_SIZE * i, a.encode())
                    flash_addr += a.flash_location.size
        flash_writer.write(flash_addr, app_data)
        new.flash_location.address = flash_addr
        new.eeprom_location.address = eeprom_addr
        flash_writer.write(FLASH_INDEX_START + FLASH_ENTRY_SIZE * flash_index_pos, new.encode())
        flash_writer.execute()
        print()

        end_time = time.time()
        print(f"DONE, app installed in {end_time - start_time:.1f} s")

    def install_all(self, app_files: List[PathLike], dont_update: bool, downgrade: bool) -> None:
        """Install all apps from files."""
        self._read_index()
        for i, app_file in enumerate(app_files):
            if len(app_files) > 1:
                print(f"INSTALLING APP {i + 1} / {len(app_files)}")
            self.install(app_file, dont_update, downgrade)
            print()

    def uninstall(self, app_id: int, clear_data: bool) -> None:
        """Uninstall the specified app, optionally clearing EEPROM data as well.
        Note that uninstalling an app only involves changing the index, no erasing is done."""
        self._read_index()
        flash_pos = self._find_app(self.flash_index, app_id)
        eeprom_pos = self._find_app(self.eeprom_index, app_id)

        # confirm uninstall
        if flash_pos != -1:
            AppManager._print_app(self.flash_index[flash_pos])
            print()
            if app_id == APP_ID_SYSTEM:
                AppManager._warn("the system app is used for programming the device. Uninstalling "
                                 "it might prevent further operation until device is reprogrammed")
            if not self._confirm("Uninstall this app and clear its data?" if clear_data
                                 else "Uninstall this app but keep its data?"):
                return
        elif eeprom_pos != -1 and clear_data:
            print("App not found on device, but EEPROM space is reserved for the app:")
            eeprom_size = self.eeprom_index[eeprom_pos].location.size
            print(f"[{app_id}] {readable_size(eeprom_size)}")
            if not self._confirm("Clear this app data?"):
                return
        else:
            print("App not found on device, nothing to do.")
            return
        print()

        # erase index entry in flash
        if flash_pos != -1:
            print("Updating flash index...")
            flash_writer = MemoryManager(self.flash)
            flash_writer.write(FLASH_INDEX_START + FLASH_ENTRY_SIZE * flash_pos,
                               bytearray(FLASH_ENTRY_SIZE))
            flash_writer.execute()
            print()

        # erase index entry in eeprom
        if eeprom_pos != -1 and clear_data:
            print("Updating EEPROM index...")
            eeprom_writer = MemoryManager(self.eeprom)
            eeprom_writer.write(EEPROM_INDEX_START + EEPROM_ENTRY_SIZE * eeprom_pos,
                                bytearray(EEPROM_ENTRY_SIZE))
            eeprom_writer.execute()
            print()

        print("DONE, app uninstalled.")

    def uninstall_all(self, app_ids: List[int], clear_data: bool) -> None:
        """Uninstall all specified app IDs, optionally clearing their EEPROM data as well."""
        self._read_index()
        for i, app_id in enumerate(app_ids):
            if len(app_ids) > 1:
                print(f"UNINSTALLING APP {i + 1} / {len(app_ids)}")
            self.uninstall(app_id, clear_data)
            print()

    def read_eeprom(self, app_id: int, filename: PathLike) -> None:
        """Read data for an app ID on device and store it in a file."""
        self._read_index()
        eeprom_pos = self._find_app(self.eeprom_index, app_id)
        if eeprom_pos != -1:
            print(f"Reading EEPROM for app {app_id}")
            app_data = self.eeprom_index[eeprom_pos]
            eeprom_data = self.eeprom.read(app_data.location.address, app_data.location.size,
                                           print_progress_bar("Reading"))
            try:
                with open(filename, "wb") as file:
                    file.write(eeprom_data)
            except IOError as e:
                raise ProgError(f"failed to write file '{filename}': {e}")
        else:
            print("App not found on device, nothing to do.")

    def write_eeprom(self, app_id: int, filename: PathLike) -> None:
        """Write data for an app ID on device from a file."""
        self._read_index()
        eeprom_pos = self._find_app(self.eeprom_index, app_id)
        if eeprom_pos != -1:
            if not self._confirm(f"Overwrite EEPROM data for app ID {app_id}?"):
                return

            try:
                with open(filename, "rb") as file:
                    data = file.read()
            except IOError as e:
                raise ProgError(f"failed to read file '{filename}': {e}")

            print(f"Writing EEPROM for app {app_id}")
            app_data = self.eeprom_index[eeprom_pos]
            # adjust data size to match app reserved eeprom size
            if len(data) > app_data.location.size:
                data = data[:app_data.location.size]
            else:
                data += bytearray(app_data.location.size - len(data))

            eeprom_writer = MemoryManager(self.eeprom)
            eeprom_writer.write(app_data.location.address, data)
            eeprom_writer.execute()
        else:
            print("App not found on device, nothing to do.")

    def erase_eeprom(self, app_id: int) -> None:
        """Erase data for an app ID on device from a file."""
        self._read_index()
        eeprom_pos = self._find_app(self.eeprom_index, app_id)
        if eeprom_pos != -1:
            if not self._confirm(f"Clear EEPROM data for app ID {app_id}?"):
                return

            print(f"Erasing EEPROM for app {app_id}")
            app_data = self.eeprom_index[eeprom_pos]
            eeprom_writer = MemoryManager(self.eeprom)
            eeprom_writer.write(app_data.location.address, bytearray(app_data.location.size))
            eeprom_writer.execute()
        else:
            print("App not found on device, nothing to do.")

    def list_all(self) -> None:
        """List and describe all the installed app on the device."""
        self._read_index()

        print("Installed apps:")
        installed = 0
        for app in self.flash_index:
            if app.app_id != APP_ID_NONE:
                print()
                AppManager._print_app(app)
                installed += 1
        if installed == 0:
            print("There are no apps installed on device.")
        print()

        eeprom_only_ids = set(a.app_id for a in self.eeprom_index if a.app_id != APP_ID_NONE) - \
                          set(a.app_id for a in self.flash_index if a.app_id != APP_ID_NONE)
        if eeprom_only_ids:
            print("EEPROM space reserved for uninstalled app IDs:")
            for app in self.eeprom_index:
                if app.app_id in eeprom_only_ids:
                    print(f"[{app.app_id}] {readable_size(app.location.size)}")
            print()

        total_flash = sum(a.flash_location.size for a in self.flash_index) + FLASH_DATA_START
        print(progress_bar("Flash usage ",
                           f"{readable_size(total_flash)} / {readable_size(FLASH_SIZE)}",
                           total_flash / FLASH_SIZE))

        total_eeprom = sum(a.location.size for a in self.eeprom_index) + EEPROM_DATA_START
        print(progress_bar("EEPROM usage",
                           f"{readable_size(total_eeprom)} / {readable_size(EEPROM_SIZE)}",
                           total_eeprom / EEPROM_SIZE))
