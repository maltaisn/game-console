
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

#ifndef COMM_H
#define COMM_H

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
 * === VERSION ===
 * - RX payload: empty
 * - TX payload:
 * [0]: version major
 * [1]: version minor
 *
 * === BATTERY ===
 * - RX payload: empty
 * - TX payload:
 * [0]: battery status (see enum values in power.h).
 * [1]: battery estimated percentage if discharging, undefined otherwise.
 * [2..3]: battery estimated voltage if discharging, undefined otherwise (little-endian, mV).
 *
 * === LED ===
 * - RX payload:
 * [0]: new LED state (1=on, others=off)
 * - No TX packet
 *
 * === INPUT ===
 * - RX payload: empty
 * - TX payload:
 * [0]: bits [0:5] indicate the state of buttons (1=pressed).
 *
 * === SPI ===
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
 * === TIME ===
 * - RX payload: empty
 * - TX payload:
 * [0..2]: system time (little-endian)
 */

typedef enum {
    PACKET_VERSION = 0x00,
    PACKET_BATTERY = 0x01,
    PACKET_LED = 0x02,
    PACKET_INPUT = 0x03,
    PACKET_SPI = 0x04,
    PACKET_TIME = 0x05,
} packet_type_t;

/**
 * Receive & decode data from RX.
 * Once a packet signature is detected, this function blocks until packet is fully received.
 */
void comm_receive(void);

/**
 * Transmit packet of `type` with a payload of `length` bytes to TX.
 */
void comm_transmit(uint8_t type, uint8_t length, const uint8_t payload[length]);

/**
 * Function called when a packet of undefined type is received.
 * Can be used to implement custom packets.
 */
void comm_undef_packet_callback(uint8_t type, uint8_t length, uint8_t payload[length]);

#endif //COMM_H
