
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

#ifdef SIMULATION

#ifndef SIM_DISPLAY_H
#define SIM_DISPLAY_H

#include <sys/display.h>

#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#define DISPLAY_COLOR_R 1.0f
#define DISPLAY_COLOR_G 1.0f
#define DISPLAY_COLOR_B 1.0f

// maximum contrast after which there's no difference
#define DISPLAY_MAX_CONTRAST 0x7f
// gap in percent between pixels
#define DISPLAY_PIXEL_GAP 0.0f

// mutex used to synchronize access to display data buffer (for full display).
// display data is updated from a different thread than the one that draws the display,
// synchronization is required to avoid rendering artifacts.
extern pthread_mutex_t display_mutex;

/**
 * Draw the display on a frame where each pixel is 1x1.
 */
void display_draw(void);

#endif //SIM_DISPLAY_H

#endif //SIMULATION