
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

#ifndef SIM_MEMORY_H
#define SIM_MEMORY_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t size;
    uint8_t initial;
    const char* filename;
    uint8_t data[];
} sim_mem_t;

/**
 * Initialize and allocate memory device data with a size and initial value.
 * This structure must be freed with `sim_mem_free` after usage.
 */
sim_mem_t* sim_mem_init(size_t size, uint8_t initial);

/**
 * Load memory from the filename set.
 */
void sim_mem_load(sim_mem_t *mem, const char* filename);

/**
 * Save memory to the filename set.
 */
void sim_mem_save(sim_mem_t *mem);

/**
 * Read a number of bytes from memory at address into a buffer.
 */
void sim_mem_read(sim_mem_t *mem, size_t address, size_t length, void* dest);

/**
 * Write a number of bytes from buffer at address to the memory.
 */
void sim_mem_write(sim_mem_t *mem, size_t address, size_t length, const void* dest);

/**
 * Free allocated memory device data.
 */
void sim_mem_free(sim_mem_t *mem);

#endif //SIM_MEMORY_H
