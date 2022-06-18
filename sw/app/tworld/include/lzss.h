
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

#ifndef TWORLD_LZSS_H
#define TWORLD_LZSS_H

#include <core/flash.h>

#include <stdint.h>

/**
 * Inflate LZSS-compressed data in the `src` buffer in Flash and write it to the `dst` buffer.
 * `length` bytes are read from the source buffer (must not end in between tokens!).
 * Notes on this particular LZSS implementation:
 *
 * - Token type is indicated by a byte, with a 0 bit indicating a raw byte and a 1 bit indicating
 *   a back reference. The first token of the stream is always a token type byte.
 *   When 8 tokens have been read, a new type token is placed.
 * - Two back reference encodings (number in parentheses indicate value subtracted from real value,
 *     all fields separated by commas are concatenaned to form 1 or 2 bytes):
 *    - 2 bytes: 8-bit distance (-1), 7-bit length (-3), 1-bit flag = 0x1
 *    - 1 byte:  5-bit distance (-1), 2-bit length (-2), 1-bit flag = 0x0
 * - 256 bytes window size
 */
void lzss_decode(flash_t src, uint16_t length, void* dst);

#endif //TWORLD_LZSS_H
