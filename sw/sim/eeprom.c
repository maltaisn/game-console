
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

#include <sim/eeprom.h>
#include <sim/memory.h>
#include <sim/time.h>

#include <sys/eeprom.h>

#include <core/eeprom.h>
#include <core/trace.h>

#include <stddef.h>
#include <stdbool.h>

#define ERASE_BYTE 0xff

#ifdef SIM_MEMORY_ABSOLUTE
#define EEPROM_SIZE SYS_EEPROM_SIZE
#else
#define EEPROM_SIZE EEPROM_RESERVED_SPACE
#endif

#define STATUS_WREN_MASK 0x02
#define STATUS_BUSY_MASK 0x01

#define PAGE_MASK 0x1f  // page size = 32

enum {
    INSTRUCTION_WREN = 0x06,
    INSTRUCTION_WRDI = 0x04,
    INSTRUCTION_RDSR = 0x05,
    INSTRUCTION_WRSR = 0x01,
    INSTRUCTION_READ = 0x03,
    INSTRUCTION_WRITE = 0x02,
};

sim_mem_t* eeprom;

static struct {
    uint8_t status;
    uint8_t instr;
    eeprom_t address;
    size_t pos;
    bool writing;
} spi_eeprom;


void sim_eeprom_init(void) {
    eeprom = sim_mem_init(EEPROM_SIZE, ERASE_BYTE);
    // in simulation, EEPROM always starts at address 0, but has the correct size.
    sys_eeprom_set_location(0, EEPROM_SIZE);
}

void sim_eeprom_free(void) {
    sim_mem_free(eeprom);
    eeprom = 0;
}

void sim_eeprom_load(const char* filename) {
    sim_mem_load(eeprom, filename);
}

void sim_eeprom_save(void) {
    sim_mem_save(eeprom);
}

void sim_eeprom_spi_transceive(size_t length, uint8_t data[static length]) {
    if (!eeprom) {
        return;
    }

    for (size_t i = 0; i < length; ++i) {
        uint8_t b = data[i];
        size_t pos = spi_eeprom.pos;

        if (spi_eeprom.instr == 0 && pos == 0) {
            // set new instruction
            spi_eeprom.instr = b;
        }

        switch (spi_eeprom.instr) {
            case INSTRUCTION_WREN: {
                spi_eeprom.status |= STATUS_WREN_MASK;
                break;
            }
            case INSTRUCTION_WRDI: {
                spi_eeprom.status &= ~STATUS_WREN_MASK;
                break;
            }
            case INSTRUCTION_RDSR: {
                if (pos == 1) {
                    data[i] = spi_eeprom.status;
                    // after write, status will read as busy once, then ready.
                    spi_eeprom.status &= ~STATUS_BUSY_MASK;
                }
                break;
            }
            case INSTRUCTION_WRSR: {
                spi_eeprom.status = b & 0x8c;
                break;
            }
            case INSTRUCTION_READ: {
                if (pos == 1) {
                    spi_eeprom.address = b << 8;
                } else if (pos == 2) {
                    spi_eeprom.address |= b;
                    spi_eeprom.address %= SYS_EEPROM_SIZE;
                } else if (pos >= 3) {
                    data[i] = eeprom->data[spi_eeprom.address];
                    spi_eeprom.address = (spi_eeprom.address + 1) % SYS_EEPROM_SIZE;
                }
                break;
            }
            case INSTRUCTION_WRITE: {
                if (pos == 0) {
                    spi_eeprom.writing = (spi_eeprom.status & STATUS_WREN_MASK) != 0;
                    spi_eeprom.status &= ~STATUS_WREN_MASK;
                    spi_eeprom.status |= STATUS_BUSY_MASK;
                } else if (pos == 1) {
                    spi_eeprom.address = b << 8;
                } else if (pos == 2) {
                    spi_eeprom.address |= b;
                    spi_eeprom.address %= SYS_EEPROM_SIZE;
                } else if (spi_eeprom.writing) {
                    eeprom->data[spi_eeprom.address] = b;
                    spi_eeprom.address = (spi_eeprom.address & ~PAGE_MASK) |
                                         ((spi_eeprom.address + 1) & PAGE_MASK);
                }
                break;
            }
            default:
                if (pos == 0) {
                    trace("unsupported EEPROM instruction 0x%02x", spi_eeprom.instr);
                }
                break;
        }

        ++spi_eeprom.pos;
    }
}

void sim_eeprom_spi_reset(void) {
    spi_eeprom.instr = 0;
    spi_eeprom.address = 0;
    spi_eeprom.pos = 0;
    spi_eeprom.writing = false;
}
