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
import random
import time
from dataclasses import dataclass
from enum import Enum
from typing import List

import numpy as np

from prog.comm import CommInterface, PacketType, Packet, ProgError
from utils import DataReader, PathLike, progress_bar

# delay before making each measurement in seconds.
# power monitoring occurs every second so 2 seconds is a minimum,
# but not enough to let the voltage stabilize. 5 seconds seems fine.
CALIB_MEASURE_DELAY = 5

# number of recent measurements used to estimate current voltage and show stats.
CALIB_RECENT_COUNT = 30

# number of values in range for each load type
CALIB_CONTRAST_COUNT = 8
CALIB_COLOR_COUNT = 16

# battery voltage at which to stop calibration.
CALIB_STOP_VOLTAGE = 3.300


def format_time(t: float) -> str:
    """Format time in seconds to 'hhhh:mm:ss' format."""
    h, r = divmod(t, 3600)
    m, s = divmod(r, 60)
    return f"{h:02.0f}:{m:02.0f}:{s:02.0f}"


def voltage_from_adc(adc: int) -> float:
    # see sys/power.c
    return adc * 6.93592e-5


class BatteryStatus(Enum):
    UNKNOWN = "unknown"
    NONE = "no battery"
    CHARGING = "charging"
    CHARGED = "charged"
    DISCHARGING = "discharging"


@dataclass
class BatteryInfo:
    status: BatteryStatus
    percent: int
    voltage: float
    adc_value: int


class BatteryManager:
    comm: CommInterface

    def __init__(self, comm: CommInterface):
        self.comm = comm

    def _get_info(self) -> BatteryInfo:
        self.comm.write(Packet(PacketType.BATTERY_INFO))
        packet = self.comm.read()
        reader = DataReader(packet.payload)
        status = list(BatteryStatus)[reader.read(1)]
        percent = reader.read(1)
        voltage = reader.read(2) / 1000
        adc_value = reader.read(2)
        return BatteryInfo(status, percent, voltage, adc_value)

    def _set_enabled(self, packet_type: PacketType, enabled: bool) -> None:
        self.comm.write(Packet(packet_type, [0xff if enabled else 0x00]))
        self.comm.read()

    def _set_load(self, contrast: int, color: int) -> None:
        if not (0 <= contrast <= 0xff):
            raise ValueError("wrong contrast value")
        if not (0 <= color <= 15):
            raise ValueError("wrong color value")
        self.comm.write(Packet(PacketType.BATTERY_LOAD, [contrast, color]))
        self.comm.read()

    def print_info(self) -> None:
        info = self._get_info()
        print(f"Status: {info.status.name}")
        print(f"Level: {info.percent}% ({info.voltage:.3f} V)")

    def _take_measurements(self, output_file: PathLike) -> None:
        file = open(output_file, "w")

        measurements = []
        start_time = time.time()
        progress = 0.0
        last_progress = None
        start_voltage = None

        def get_recent_voltage() -> List[float]:
            recent = measurements[-min(len(measurements), CALIB_RECENT_COUNT):]
            return [voltage_from_adc(m[1]) for m in recent]

        def show_progress() -> None:
            curr_time = time.time() - start_time
            right = format_time(curr_time)
            if last_progress is not None and progress < 1:
                recent_voltage = get_recent_voltage()
                right += f", min = {min(recent_voltage):.3f} V, " \
                         f"max = {max(recent_voltage):.3f} V, " \
                         f"avg = {np.average(recent_voltage):.3f} V, " \
                         f"n = {len(measurements)}"
            print("\033[2K", end="")  # erase previous bar
            print(progress_bar("Discharging", right, progress), end="\r")

        show_progress()

        all_contrasts = np.linspace(0, 255, CALIB_CONTRAST_COUNT, dtype=np.uint8)
        all_colors = np.linspace(0, 15, CALIB_COLOR_COUNT, dtype=np.uint8)

        while progress < 1:
            # take measurement
            contrast = random.choice(all_contrasts)
            color = random.choice(all_colors)
            self._set_load(contrast, color)

            measure_start = time.time()
            while time.time() - measure_start < CALIB_MEASURE_DELAY:
                show_progress()
                time.sleep(0.2)

            info = self._get_info()
            if info.status != BatteryStatus.DISCHARGING:
                raise ProgError("battery must be discharging to do calibration")
            measurement = (time.time() - start_time, info.adc_value, contrast, color)
            measurements.append(measurement)

            file.write(','.join(str(v) for v in measurement))
            file.write("\n")
            file.flush()

            # estimate current battery voltage from recent measurements
            curr_voltage = np.average(get_recent_voltage())
            if start_voltage is None:
                if len(measurements) > CALIB_RECENT_COUNT:
                    start_voltage = curr_voltage
                    last_progress = 0.0
                progress = 0.0
            else:
                progress = max(last_progress, min(1.0, 1 - (curr_voltage - CALIB_STOP_VOLTAGE) /
                                                  (start_voltage - CALIB_STOP_VOLTAGE)))
            last_progress = progress
            show_progress()

        # set small load to avoid any further discharge
        self._set_load(0x7f, 0)

        print()
        file.close()

    def perform_calibration(self, output_file: PathLike) -> None:
        if self._get_info().status != BatteryStatus.DISCHARGING:
            raise ProgError("battery must be discharging to do calibration")

        self._set_enabled(PacketType.SLEEP, False)
        self._set_enabled(PacketType.BATTERY_CALIB, True)

        self._take_measurements(output_file)

        self._set_enabled(PacketType.BATTERY_CALIB, False)
        self._set_enabled(PacketType.SLEEP, True)
