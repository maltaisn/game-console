#  Copyright 2021 Nicolas Maltais
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

import time
from dataclasses import dataclass
from enum import Enum
from typing import Callable, Optional

from comm import Packet, PacketType, CommError, CommInterface


class BatteryStatus(Enum):
    UNKNOWN = 0, "unknown"
    NONE = 1, "no battery"
    CHARGING = 2, "charging"
    CHARGED = 3, "complete"
    DISCHARGING = 4, "discharging"

    def __init__(self, status: int, title: str):
        self.status = status
        self.title = title


@dataclass
class BatteryInfo:
    status: BatteryStatus
    percent: Optional[int]
    voltage: Optional[float]


def format_time(t: float) -> str:
    """Format time in seconds to 'hhhh:mm:ss' format."""
    h, r = divmod(t, 60)
    m, s = divmod(t, 60)
    return f"{h:02.0f}:{m:02.0f}:{s:02.0f}"


class BatteryMonitor:
    """Class used for battery monitoring."""
    comm: CommInterface

    def __init__(self, comm: CommInterface):
        self.comm = comm

    def get_info(self) -> BatteryInfo:
        """Request battery status, percent level & voltage from firmware."""
        self.comm.write(Packet(PacketType.BATTERY))
        resp = self.comm.read(4).payload
        status = next((s for s in BatteryStatus if s.status == resp[0]), None)
        if status is None:
            raise CommError("unexpected battery status value")
        if status == BatteryStatus.DISCHARGING:
            percent = resp[1]
            voltage = (resp[2] | resp[3] << 8) / 1000
        else:
            percent = None
            voltage = None
        return BatteryInfo(status, percent, voltage)

    def _monitor(self, period: float, callback: Callable[[float, BatteryInfo], None]) -> None:
        start_time = time.time()
        last_time = start_time
        while True:
            info = self.get_info()
            elapsed = time.time() - start_time
            callback(elapsed, info)
            while time.time() - last_time < period:
                time.sleep(0.1)
            last_time = time.time()

    def monitor_to_stdout(self, period: float) -> None:
        """Monitor battery level at a certain interval in seconds,
        printing results to the standard output."""
        print("Elapsed time      Status       Level   Voltage (V)")

        def callback(elapsed: float, info: BatteryInfo) -> None:
            print(f"{format_time(elapsed):^12}    {info.status.title:^11}", end="")
            if info.status == BatteryStatus.DISCHARGING:
                print(f"     {info.percent / 100:^4.0%}   {info.voltage:^11.3f}")
            else:
                print()

        self._monitor(period, callback)

    def monitor_to_csv(self, period: float, filename: str) -> None:
        """Monitor battery level at a certain interval in seconds,
        saving results to the specified CSV file."""
        try:
            with open(filename, "w") as file:
                def callback(elapsed: float, info: BatteryInfo) -> None:
                    cols = [f"{elapsed:.0f}", info.status.title]
                    if info.status == BatteryStatus.DISCHARGING:
                        cols += [f"{info.percent / 100:.2f}", f"{info.voltage:.3f}"]
                    file.write(','.join(cols))
                    file.write('\n')
                    file.flush()

                file.write("Elapsed time,Status,Level,Voltage\n")
                self._monitor(period, callback)
        except IOError as e:
            raise CommError(f"could not write to file: {e}")
