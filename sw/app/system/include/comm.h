
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

#ifndef SYSTEM_COMM_H
#define SYSTEM_COMM_H

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
 * VERSION: get version info
 * - RX payload: empty
 * - TX payload:
 * [0..1]: system app version
 * [2..3]: bootloader version
 * [4..5]: gcprog first compatible version
 *
 * SPI: transfer data on the SPI bus
 * RX & TX packets have an identical format.
 * [0]: bits [0:1] indicate the selected peripheral
 *      - 0x0: flash memory
 *      - 0x1: eeprom memory
 *      - 0x2: oled display
 *      - 0x3: reserved
 *      bit [7] is 1 if this is the last transfer, which means the CS line will be
 *      released at the end of transfer. This bit must absolutely be set for the last transfer,
 *      otherwise there might be two CS lines asserted on the next transfer!
 * [1..n]: SPI data
 *
 * FAST MODE: enable or disable fast mode
 * - RX payload:
 * [0]: 0 to disable fast mode, others to enable
 * - TX payload: empty
 * The TX packet serves as an acknoledgement that the speed has been changed,
 * and is transmitted at the new baud rate.
 * The fast mode should not be enabled twice!
 */

typedef enum {
    PACKET_VERSION = 0x00,
    PACKET_SPI = 0x01,
    PACKET_FAST_MODE = 0x02,
} packet_type_t;

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

#endif //SYSTEM_COMM_H
