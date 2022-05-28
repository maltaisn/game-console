
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

#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// Number of pixels in width
#define DISPLAY_WIDTH 128
// Number of pixels in height
#define DISPLAY_HEIGHT 128

#define DISPLAY_COLOR_BLACK ((disp_color_t) 0)
#define DISPLAY_COLOR_WHITE ((disp_color_t) 15)

/** Display generic coordinate. */
typedef uint8_t disp_coord_t;
/** Display X coordinate, 0 to (DISPLAY_WIDTH-1). */
typedef disp_coord_t disp_x_t;
/** Display Y coordinate, 0 to (DISPLAY_HEIGHT-1). */
typedef disp_coord_t disp_y_t;
/** Display "color" (grayscale level). */
typedef uint8_t disp_color_t;

/**
 * Set whether the display is inverted or not.
 */
void display_set_inverted(bool inverted);

/**
 * Set the display contrast. The default is DISPLAY_DEFAULT_CONTRAST.
 * Nothing is done if contrast is already at set value.
 */
void display_set_contrast(uint8_t contrast);

/**
 * Get the display contrast.
 */
uint8_t display_get_contrast(void);

#include <sim/display.h>

#endif //CORE_DISPLAY_H
