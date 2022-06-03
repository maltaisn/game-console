
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

#define LOADED_APP_NONE 0xff

/**
 * Load all app entries compatible with this bootloader version from the index.
 * Note that after calling this function, the currently loaded app ID must be checked before
 * accessing any boot-only variables as bootloader may not be active anymore.
 */
void load_read_index(void);

/**
 * Load the app at a position in the flash index.
 * The app is copied in program memory, checksum is checked, then setup callback is called.
 * If checksum check fails, no app is loaded and the bootloader remains active.
 * Note that after calling this function, the currently loaded app ID must be checked before
 * accessing any boot-only variables as bootloader may not be active anymore.
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

/**
 * Returns the position of the currently loaded app (the one written in flash) in the index.
 * If no app in the index is currently loaded, returns `LOADED_APP_NONE`.
 */
uint8_t load_get_loaded_app_index(void);

#endif //LOAD_H
