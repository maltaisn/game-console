/*
 * Copyright 2022 Nicolas Maltais
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

#include "lzss.h"

#define BUFFER_SIZE 16
#define BUFFER_FILL_SIZE 3

#define DISTANCE_BITS1 5
#define DISTANCE_BITS2 8

#define LENGTH_BITS1 (7 - DISTANCE_BITS1)
#define LENGTH_BITS2 (15 - DISTANCE_BITS2)

#define BREAKEVEN1 2
#define BREAKEVEN2 3

#define LENGTH_MASK1 ((1 << LENGTH_BITS1) - 1)
#define LENGTH_MASK2 ((1 << LENGTH_BITS2) - 1)

void lzss_decode(flash_t src, uint16_t length, void* dst) {
    uint8_t* out = dst;

    uint8_t buf[BUFFER_SIZE];
    uint8_t buf_pos = sizeof buf;

    uint8_t type_byte = 0;
    uint8_t type_bits = 0;
    while (length) {
        if (buf_pos > sizeof buf - BUFFER_FILL_SIZE) {
            // Source data buffer is empty or almost empty, read more data.
            // The buffer is filled even when there's still bytes left,
            // because up to 3 bytes can be read in one iteration (type token + long backref token)
            // Read (buf_pos) bytes and move (sizeof buf - buf_pos) bytes to the start of buffer.
            uint8_t i = 0;
            uint8_t pos = buf_pos;
            while (pos < sizeof buf) {
                buf[i++] = buf[pos++];
            }
            flash_read(src, buf_pos, buf + i);
            src += buf_pos;
            buf_pos = 0;
        }

        if (type_bits == 0) {
            // type token (for next 8 data tokens)
            type_byte = buf[buf_pos++];
            type_bits = 8;
            --length;
        }

        uint8_t b = buf[buf_pos++];
        --length;
        if (type_byte & 1) {
            // back reference token
            uint8_t reflen;
            const uint8_t *ref = out;
            if (b & 0x1) {
                // two bytes encoding
                uint16_t backref = (b | buf[buf_pos++] << 8) >> 1;
                reflen = (backref & LENGTH_MASK2) + BREAKEVEN2;
                ref -= backref >> LENGTH_BITS2;
                --length;
            } else {
                // single byte encoding
                uint8_t backref = b >> 1;
                reflen = (backref & LENGTH_MASK1) + BREAKEVEN1;
                ref -= backref >> LENGTH_BITS1;
            }
            --ref;
            while (reflen--) {
                *out++ = *ref++;
            }

        } else {
            // byte token
            *out++ = b;
        }

        type_byte >>= 1;
        --type_bits;
    }
}
