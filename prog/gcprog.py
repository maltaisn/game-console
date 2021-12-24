#!/usr/bin/env python3

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

# Command line program used to program and debug the game console firmware.
# The game console is connected via a UART link, which is connected to a USB serial device.
# This program can:
# - Check the firmware version.
# - Check the buttons state.
# - Check and monitor battery status.
# - Change the status LED state.
# - Get the system time.
# - Read & write to EEPROM
# - Read & write to flash
#
# Usage:
#  ./gcprog.py -v
#  ./gcprog.py --help
#  ./gcprog.py <command> --help
#  ./gcprog.py [options] <command> [command options]
#
# At least Python 3.7 is required.

import argparse
import signal
import sys
import time
from dataclasses import dataclass
from typing import Optional

import eeprom
import flash
import memory
from battery import BatteryMonitor
from comm import Comm, CommError, Packet, PacketType, CommInterface
from spi import SpiInterface

VERSION_MAJOR = 0
VERSION_MINOR = 1

BUTTONS_COUNT = 6
SYSTICK_FREQ = 256

STD_IO = "-"

parser = argparse.ArgumentParser(description="Programming & debugging program for game console")
parser.add_argument(
    "-D", "--device", action="store", type=str, dest="device", default=Comm.DEFAULT_FILENAME,
    help=f"UART device filename (default is {Comm.DEFAULT_FILENAME})")
parser.add_argument(
    "-b", "--baud", action="store", type=int, dest="baud_rate", default=Comm.DEFAULT_BAUD_RATE,
    help=f"Baud rate (default is {Comm.DEFAULT_BAUD_RATE})")
parser.add_argument(
    "-v", "--version", action="store_true", dest="version",
    help="Show version info")

subparsers = parser.add_subparsers(dest="command")

# battery command
battery_parser = subparsers.add_parser("battery", help="Get battery status")
battery_parser.add_argument(
    "-m", "--monitor", action="store", type=str, dest="monitor", nargs="?", default="", const="-",
    help="Monitor battery status continuously, optionally saving data to CSV file")
battery_parser.add_argument(
    "-p", "--period", action="store", type=float, dest="period", default=5,
    help="Battery monitoring sampling period in seconds (default is 5)")

# led command
led_parser = subparsers.add_parser("led", help="Status LED control")
led_parser.add_argument(
    action="store", type=str, dest="state", choices=["on", "off"],
    help="LED state")

# input command
input_parser = subparsers.add_parser("input", help="Get input state")
input_parser.add_argument(
    "-c", "--continuous", action="store_true", dest="continuous",
    help="Get continuous updates on input state")

# time command
time_parser = subparsers.add_parser("time", help="Get system time")

# eeprom command
memory.create_parser(subparsers, eeprom.EEPROM_SIZE, "eeprom")

# flash command
memory.create_parser(subparsers, flash.FLASH_SIZE, "flash")


@dataclass
class Version:
    major: int
    minor: int

    def __repr__(self) -> str:
        return f"v{self.major}.{self.minor}"


class Prog(CommInterface):
    """Class used to interpret command line arguments and execute commands."""
    args: argparse.Namespace
    comm: Comm
    fw_version: Optional[Version]

    interrupted: bool
    operation_in_progress: bool

    VERSION = Version(VERSION_MAJOR, VERSION_MINOR)

    def __init__(self, args):
        self.args = args
        self.interrupted = False
        self.operation_in_progress = False
        signal.signal(signal.SIGINT, self.sigint_handler)

        # connect to serial device
        self.comm = Comm(args.device, args.baud_rate)
        self.fw_version = None

    def do_command(self) -> None:
        try:
            self.comm.connect()
        except CommError as e:
            if not self.args.version:
                raise CommError(e)

        self.get_version()
        if self.args.version:
            self.print_version()
            return
        self.check_version()

        cmd = self.args.command
        if cmd == "battery":
            self.command_battery()
        elif cmd == "led":
            self.command_led()
        elif cmd == "input":
            self.command_input()
        elif cmd == "time":
            self.command_time()
        elif cmd == "eeprom":
            self.command_eeprom()
        elif cmd == "flash":
            self.command_flash()

        self.comm.disconnect()

    def get_version(self) -> None:
        if self.comm.is_connected():
            self.write(Packet(PacketType.VERSION))
            version = self.read(2).payload
            self.fw_version = Version(version[0], version[1])

    def check_version(self) -> None:
        # get firmware version and compare
        if self.fw_version.major != Prog.VERSION.major:
            raise CommError(f"gcprog version ({Prog.VERSION}) is "
                            f"incompatible with firmware ({self.fw_version})")
        elif self.fw_version.minor != Prog.VERSION.minor:
            print(f"gcprog version ({Prog.VERSION}) is "
                  f"different from firmware ({self.fw_version})")

    def print_version(self) -> None:
        print(f"gcprog {Prog.VERSION}")
        if self.fw_version:
            print(f"firmware {self.fw_version}")

    def write(self, packet: Packet) -> None:
        """Packet write wrapper around Comm.write to ensure that write operation
        is not interrupted and to ensure that RX is flushed afterwards if interrupted."""
        self.operation_in_progress = True
        self.comm.write(packet)
        if self.interrupted:
            # a packet was possibly returned, discard it (introduces a small delay).
            self.comm.serial.readall()
            exit(1)
        self.operation_in_progress = False

    def read(self, payload_size: Optional[int] = None) -> Packet:
        """Packet read wrapper around Comm.read to ensure that read operation is not interrupted."""
        self.operation_in_progress = True
        packet = self.comm.read(payload_size)
        if self.interrupted:
            exit(1)
        self.operation_in_progress = False
        return packet

    def sigint_handler(self, signum, frame):
        if self.operation_in_progress:
            self.interrupted = True
        else:
            self.comm.disconnect()
            exit(1)

    def command_led(self) -> None:
        self.write(Packet(PacketType.LED, [0x01 if self.args.state == "on" else 0x00]))

    def command_battery(self) -> None:
        monitor = BatteryMonitor(self)
        if self.args.monitor:
            period = self.args.period
            if period < 0.2:
                raise CommError("Monitoring period must be at least 1 s")
            if self.args.monitor == STD_IO:
                monitor.monitor_to_stdout(self.args.period)
            else:
                monitor.monitor_to_csv(self.args.period, self.args.monitor)
        else:
            info = monitor.get_info()
            print(f"Battery status: {info.status.title}")
            if info.percent:
                print(f"Battery percent: {info.percent} %")
            if info.voltage:
                print(f"Battery voltage: {info.voltage:.3f} V")

    def command_time(self) -> None:
        self.write(Packet(PacketType.TIME))
        resp = self.read(3).payload
        systick = resp[0] | resp[1] << 8 | resp[2] << 16
        systime = systick / SYSTICK_FREQ
        print(f"System time is {systick} ({systime:.2f} s)")

    def command_input(self) -> None:
        tx_packet = Packet(PacketType.INPUT)
        # print instantaneous input state
        self.write(tx_packet)
        last_state = self.read(1).payload[0]
        for i in range(BUTTONS_COUNT):
            if last_state & (1 << i):
                print(f"button {i} pressed")
        if self.args.continuous:
            # print input state changes
            while True:
                self.write(tx_packet)
                curr_state = self.read(1).payload[0]
                if last_state is not None:
                    for i in range(BUTTONS_COUNT):
                        last_pressed = last_state & (1 << i)
                        curr_pressed = curr_state & (1 << i)
                        if curr_pressed and not last_pressed:
                            print(f"button {i} pressed")
                        elif not curr_pressed and last_pressed:
                            print(f"button {i} released")
                last_state = curr_state
                time.sleep(0.03)

    def command_eeprom(self) -> None:
        spi = SpiInterface(self)
        driver = eeprom.EepromDriver(spi)
        config = memory.create_config(eeprom.EEPROM_SIZE, self.args)
        memory.execute_config(driver, config)

    def command_flash(self) -> None:
        spi = SpiInterface(self)
        driver = flash.FlashDriver(spi)
        config = memory.create_config(flash.FLASH_SIZE, self.args)
        memory.execute_config(driver, config)


def main() -> None:
    args = parser.parse_args()
    try:
        prog = Prog(args)
        prog.do_command()
    except CommError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        exit(1)


if __name__ == '__main__':
    main()
