
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

#include "core/debug.h"
#include "core/comm.h"

static const char HEX_CHARS[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};

void debug_print(const char* str) {
    while (*str) {
        uint8_t len = 0;
        uint8_t* buf = comm_payload_buf;
        while (*str && len < PAYLOAD_MAX_SIZE) {
            *buf++ = *str++;
            ++len;
        }
        comm_transmit(PACKET_DEBUG, len);
    }
}

void debug_println(void) {
    comm_payload_buf[0] = '\n';
    comm_transmit(PACKET_DEBUG, 1);
}

static void print_hex8(uint8_t pos, uint8_t n) {
    comm_payload_buf[pos++] = HEX_CHARS[n >> 4];
    comm_payload_buf[pos] = HEX_CHARS[n & 0xf];
}

void debug_print_hex8(uint8_t n) {
    print_hex8(0, n);
    comm_transmit(PACKET_DEBUG, 2);
}

void debug_print_hex32(uint32_t n) {
    print_hex8(0, n >> 24);
    print_hex8(2, (n >> 16) & 0xff);
    print_hex8(4, (n >> 8) & 0xff);
    print_hex8(6, n & 0xff);
    comm_transmit(PACKET_DEBUG, 8);
}
