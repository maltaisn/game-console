
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

#include <sim/memory.h>

#include <core/trace.h>

#include <memory.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>

#define READ_BUFFER_SIZE 8192
#define WRITE_BUFFER_SIZE 8192

sim_mem_t* sim_mem_init(size_t size, uint8_t initial) {
    size_t mem_size = sizeof(sim_mem_t) + size;
    sim_mem_t* mem = malloc(mem_size);
    if (mem == 0) {
        trace("out of memory");
        return 0;
    }
    memset(mem, initial, mem_size);
    mem->size = size;
    mem->initial = initial;
    mem->filename = 0;
    return mem;
}

void sim_mem_load(sim_mem_t* mem, const char* filename) {
    mem->filename = filename;
    FILE* file = fopen(filename, "rb");
    if (!file || ferror(file)) {
        trace("could not read memory file '%s'", filename);
        return;
    }

    uint8_t* ptr = mem->data;
    while (true) {
        size_t n = READ_BUFFER_SIZE;
        if (ptr + n >= mem->data + mem->size) {
            n = (size_t) (mem->size - (ptrdiff_t) ptr + mem->data);
        }
        size_t read = fread(ptr, 1, n, file);
        ptr += read;
        if (read < READ_BUFFER_SIZE) {
            // end of file reached, short count.
            break;
        }
        if (ptr >= mem->data + mem->size) {
            // end of memory reached.
            break;
        }
    }

    // erase the rest of memory
    memset(ptr, mem->initial, mem->size - (ptr - mem->data));

    fclose(file);
}


void sim_mem_save(sim_mem_t* mem) {
    if (!mem->filename) {
        return;
    }

    FILE* file = fopen(mem->filename, "wb");
    if (!file || ferror(file)) {
        return;
    }

    // check up to which point to write.
    // all the uninitialized part at the end is not written.
    size_t write_size = 0;
    for (size_t i = 0; i < mem->size; ++i) {
        if (mem->data[i] != mem->initial) {
            write_size = i + 1;
        }
    }

    uint8_t* ptr = mem->data;
    while (true) {
        size_t n = WRITE_BUFFER_SIZE;
        if (ptr + n >= mem->data + write_size) {
            n = (size_t) (write_size - (ptrdiff_t) ptr + mem->data);
        }
        size_t written = fwrite(ptr, 1, n, file);
        if (written < WRITE_BUFFER_SIZE) {
            // short count, unknown error occured.
            break;
        }
        ptr += written;
        if (ptr >= mem->data + mem->size) {
            // end of EEPROM reached.
            break;
        }
    }

    fclose(file);
}

void sim_mem_read(sim_mem_t *mem, size_t address, size_t length, void* dest) {
    if (address >= mem->size) {
        return;
    } else if (address + length >= mem->size) {
        length = mem->size - address;
    }
    memcpy(dest, &mem->data[address], length);
}

void sim_mem_write(sim_mem_t *mem, size_t address, size_t length, const void* src) {
    if (address >= mem->size) {
        return;
    } else if (address + length >= mem->size) {
        length = mem->size - address;
    }
    memcpy(&mem->data[address], src, length);
}

void sim_mem_free(sim_mem_t* mem) {
    free(mem);
}
