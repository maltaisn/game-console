
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

#include <sim/flash.h>
#include <sim/memory.h>
#include <sys/flash.h>

#include <core/trace.h>

#include <stddef.h>
#include <stdbool.h>
#include <memory.h>
#include <pthread.h>

#define READ_BUFFER_SIZE 8192
#define ERASE_BYTE 0xff

#define STATUS_WREN_MASK 0x02
#define STATUS_BUSY_MASK 0x01

#define PAGE_MASK 0xff  // page size = 256

enum {
    INSTRUCTION_READ = 0x03,
    INSTRUCTION_WRITE = 0x02,
    INSTRUCTION_ERASE_4KB = 0x20,
    INSTRUCTION_ERASE_32KB = 0x52,
    INSTRUCTION_ERASE_64KB = 0xd8,
    INSTRUCTION_ERASE_ALL = 0x60,
    INSTRUCTION_WRITE_ENABLE = 0x06,
    INSTRUCTION_WRITE_DISABLE = 0x04,
    INSTRUCTION_READ_STATUS = 0x05,
    INSTRUCTION_WRITE_STATUS = 0x01,
    INSTRUCTION_RESET_ENABLE = 0x66,
    INSTRUCTION_RESET = 0x99,
    INSTRUCTION_POWER_DOWN_ENABLE = 0xb9,
    INSTRUCTION_POWER_DOWN_DISABLE = 0xab,
    INSTRUCTION_MANUF_ID = 0x9f,
};

static const uint8_t MANUFACTURER_ID[] = {0x1f, 0x85, 0x01};

sim_mem_t* flash;
pthread_mutex_t flash_mutex;

static struct {
    uint8_t status;
    uint8_t instr;
    flash_t address;
    size_t pos;
    bool writing;
    bool reset_enabled;
    bool power_down;
} spi_flash;

void sim_flash_init(void) {
    flash = sim_mem_init(SYS_FLASH_SIZE, ERASE_BYTE);
}

void sim_flash_free(void) {
    pthread_mutex_lock(&flash_mutex);
    sim_mem_free(flash);
    flash = 0;
    pthread_mutex_unlock(&flash_mutex);
}

void sim_flash_load(const char* filename) {
    pthread_mutex_lock(&flash_mutex);
    sim_mem_load(flash, filename);
    pthread_mutex_unlock(&flash_mutex);
    // in simulation, flash always starts at address 0
    sys_flash_set_offset(0);
}

void sim_flash_save(void) {
    pthread_mutex_lock(&flash_mutex);
    sim_mem_save(flash);
    pthread_mutex_unlock(&flash_mutex);
}

void sim_flash_spi_transceive(size_t length, uint8_t data[static length]) {
    pthread_mutex_lock(&flash_mutex);
    if (!flash) {
        return;
    }

    for (size_t i = 0; i < length; ++i) {
        uint8_t b = data[i];
        size_t pos = spi_flash.pos;

        if (spi_flash.instr == 0 && pos == 0) {
            if (spi_flash.power_down && b != INSTRUCTION_POWER_DOWN_DISABLE) {
                // flash is in deep power-down, all commands ignored except wakeup.
                trace("instruction 0x%02x ignored in power-down mode.", b);
                return;
            }
            // set new instruction
            spi_flash.instr = b;
        }

        switch (spi_flash.instr) {
            case INSTRUCTION_READ: {
                if (pos == 1) {
                    spi_flash.address = b << 16;
                } else if (pos == 2) {
                    spi_flash.address |= b << 8;
                } else if (pos == 3) {
                    spi_flash.address |= b;
                    spi_flash.address %= SYS_FLASH_SIZE;
                } else if (pos >= 4) {
                    data[i] = flash->data[spi_flash.address];
                    spi_flash.address = (spi_flash.address + 1) % SYS_FLASH_SIZE;
                }
                break;
            }
            case INSTRUCTION_WRITE: {
                if (pos == 0) {
                    spi_flash.writing = (spi_flash.status & STATUS_WREN_MASK) != 0;
                    spi_flash.status &= ~STATUS_WREN_MASK;
                    spi_flash.status |= STATUS_BUSY_MASK;
                } else if (pos == 1) {
                    spi_flash.address = b << 16;
                } else if (pos == 2) {
                    spi_flash.address |= b << 8;
                } else if (pos == 3) {
                    spi_flash.address |= b;
                    spi_flash.address %= SYS_FLASH_SIZE;
                } else if (spi_flash.writing) {
                    // data AND with existing data like the real flash device.
                    flash->data[spi_flash.address] &= b;
                    spi_flash.address = (spi_flash.address & ~PAGE_MASK) |
                                        ((spi_flash.address + 1) & PAGE_MASK);
                }
                break;
            }
            case INSTRUCTION_ERASE_4KB:
            case INSTRUCTION_ERASE_32KB:
            case INSTRUCTION_ERASE_64KB: {
                if (pos == 0) {
                    spi_flash.writing = (spi_flash.status & STATUS_WREN_MASK) != 0;
                    spi_flash.status &= ~STATUS_WREN_MASK;
                    spi_flash.status |= STATUS_BUSY_MASK;
                } else if (pos == 1) {
                    spi_flash.address = b << 16;
                } else if (pos == 2) {
                    spi_flash.address |= b << 8;
                } else if (pos == 3) {
                    spi_flash.address |= b;
                    spi_flash.address %= SYS_FLASH_SIZE;

                    // erase the block
                    size_t size;
                    if (spi_flash.instr == INSTRUCTION_ERASE_4KB) {
                        size = 0x1000;
                    } else if (spi_flash.instr == INSTRUCTION_ERASE_32KB) {
                        size = 0x8000;
                    } else {
                        size = 0x10000;
                    }
                    spi_flash.address = spi_flash.address & ~(size - 1);
                    memset(&flash->data[spi_flash.address], ERASE_BYTE, size);
                }
                break;
            }
            case INSTRUCTION_ERASE_ALL: {
                if (pos == 0) {
                    if (spi_flash.status == STATUS_WREN_MASK) {
                        memset(flash->data, ERASE_BYTE, SYS_FLASH_SIZE);
                    }
                    spi_flash.status &= ~STATUS_WREN_MASK;
                    spi_flash.status |= STATUS_BUSY_MASK;
                }
                break;
            }
            case INSTRUCTION_WRITE_ENABLE: {
                spi_flash.status |= STATUS_WREN_MASK;
                break;
            }
            case INSTRUCTION_WRITE_DISABLE: {
                spi_flash.status &= ~STATUS_WREN_MASK;
                break;
            }
            case INSTRUCTION_READ_STATUS: {
                if (pos == 1) {
                    data[i] = spi_flash.status;
                    // after write, status will read as busy once, then ready.
                    spi_flash.status &= ~STATUS_BUSY_MASK;
                }
                break;
            }
            case INSTRUCTION_WRITE_STATUS: {
                if (pos == 1) {
                    spi_flash.status = b & 0xf4;
                }
                break;
            }
            case INSTRUCTION_RESET_ENABLE: {
                spi_flash.reset_enabled = true;
                break;
            }
            case INSTRUCTION_RESET: {
                if (pos == 0 && spi_flash.reset_enabled) {
                    spi_flash.status = 0;
                    spi_flash.reset_enabled = false;
                }
                break;
            }
            case INSTRUCTION_POWER_DOWN_ENABLE: {
                spi_flash.power_down = true;
                break;
            }
            case INSTRUCTION_POWER_DOWN_DISABLE: {
                spi_flash.power_down = false;
                break;
            }
            case INSTRUCTION_MANUF_ID: {
                if (pos > 0 && pos <= sizeof MANUFACTURER_ID) {
                    data[i] = MANUFACTURER_ID[spi_flash.pos - 1];
                }
                break;
            }
            default:
                if (pos == 0) {
                    trace("unsupported flash instruction 0x%02x", spi_flash.instr);
                }
                break;
        }

        ++spi_flash.pos;
    }

    pthread_mutex_unlock(&flash_mutex);
}

void sim_flash_spi_reset(void) {
    spi_flash.instr = 0;
    spi_flash.address = 0;
    spi_flash.pos = 0;
    spi_flash.writing = false;
}
