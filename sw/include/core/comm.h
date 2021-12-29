
/*
 * Copyright 2021 Nicolas Maltais
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORE_COMM_H
#define CORE_COMM_H

#include <stdint.h>

#define PACKET_SIGNATURE 0xc3
#define PAYLOAD_MAX_SIZE 0xff

/*
 * This module defines an interface for communicating to the game console via the UART link.
 * There are packets defined for several subsystems to allow remote programming & debugging.
 * Each packet transmitted and received has the following format:
 *
 * [0]: signature byte 0xc3
 * [1]: packet type
 * [2]: payload length n (0-255)
 * [3..(n+3)]: payload
 *
 * The payload of different packet types defined are described below.
 * "RX" refers to the receiving side of the game console and "TX" refers to the transmitting side.
 *
 * VERSION: get the firmware version
 * - RX payload: empty
 * - TX payload:
 * [0]: version major
 * [1]: version minor
 *
 * BATTERY: get the battery status
 * - RX payload: empty
 * - TX payload:
 * [0]: battery status (see enum values in power.h).
 * [1]: battery estimated percentage if discharging, undefined otherwise.
 * [2..3]: battery estimated voltage if discharging, undefined otherwise (little-endian, mV).
 *
 * LED: change status LED state
 * - RX payload:
 * [0]: new LED state (1=on, others=off)
 * - No TX packet
 *
 * INPUT: get input state
 * - RX payload: empty
 * - TX payload:
 * [0]: bits [0:5] indicate the state of buttons (1=pressed).
 *
 * SPI: transfer data on the SPI bus
 * RX & TX packets have an identical format.
 * [0]: bits [0:1] indicate the selected peripheral
 *      - 00: flash memory
 *      - 01: eeprom memory
 *      - 10: oled display
 *      - 11: reserved
 *      bit [7] is 1 if this is the last transfer, which means the CS line will be
 *      released at the end of transfer. This bit must absolutely be set for the last transfer,
 *      otherwise there might be two CS lines asserted on the next transfer!
 * [1..n]: SPI data
 *
 * TIME: get system time
 * - RX payload: empty
 * - TX payload:
 * [0..2]: system time (little-endian)
 *
 * FAST MODE: enable or disable fast mode
 * - RX payload:
 * [0]: 0 to disable fast mode, others to enable
 * - TX payload: empty
 * The TX packet serves as an acknoledgement that the speed has been changed,
 * and is transmitted at the new baud rate.
 * The fast mode should not be enabled twice!
 *
 * DEBUG: used by firmware to send debug info
 * - No RX packet
 * - TX payload:
 * [0..n]: debug data / message
 *
 * RESET: trigger software reset
 * - RX payload: empty
 * - No TX packet
 */

typedef enum {
    PACKET_VERSION = 0x00,
    PACKET_BATTERY = 0x01,
    PACKET_LED = 0x02,
    PACKET_INPUT = 0x03,
    PACKET_SPI = 0x04,
    PACKET_TIME = 0x05,
    PACKET_FAST_MODE = 0x06,
    PACKET_DEBUG = 0x07,
    PACKET_RESET = 0x08,
} packet_type_t;

/**
 * Buffer used to store the packet payload on receive and transmit.
 */
extern uint8_t comm_payload_buf[PAYLOAD_MAX_SIZE];

/**
 * Receive & decode data from RX.
 * Once a packet signature is detected, this function blocks until packet is fully received.
 * If a fast mode enable packet is received, this function blocks until fast mode is disabled.
 * Must not be called with interrupts enabled.
 */
void comm_receive(void);

/**
 * Transmit packet of `type` with a payload of `length` bytes to TX.
 * The payload is contained in the `comm_payload_buf` buffer.
 */
void comm_transmit(uint8_t type, uint8_t length);

/**
 * Function called when a packet of undefined type is received.
 * Can be used to implement custom packets.
 * The payload is contained in the `comm_payload_buf` buffer and has a `length`.
 */
void comm_undef_packet_callback(uint8_t type, uint8_t length);

#endif //CORE_COMM_H
