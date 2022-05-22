
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

#ifndef LOAD_H
#define LOAD_H

#include <stdint.h>
#include <sys/flash.h>
#include <core/graphics.h>

// This app ID is used to identify the bootloader.
// It's also used to identify when no app is present.
#define APP_ID_NONE 0

/**
 * Load all app entries compatible with this bootloader version from the index.
 */
void load_read_index(void);

/**
 * Returns the ID of the loaded app, or `APP_BOOTLOADER` if none is loaded.
 */
uint8_t load_get_loaded(void);

/**
 * Load the app at a position in the flash index.
 * The app is copied in program memory, checksum is checked, then setup callback is called.
 * After returning from this function, boot-only variables must not be used.
 */
void _load_app(uint8_t index);

/**
 * Returns the app image. The image is an absolute address in the flash memory space,
 * so the flash offset should be zero when this is used.
 */
graphics_image_t load_get_app_image(uint8_t index);

/**
 * Returns the number of apps in the flash index.
 */
uint8_t load_get_app_count(void);

#endif //LOAD_H
