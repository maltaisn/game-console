
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

#ifndef CORE_FPSMON_H
#define CORE_FPSMON_H

/**
 * Draw FPS monitor in the lower left corner of the display.
 * This also updates the monitor state when on the first display page.
 * The FPS monitor uses 4 bytes of RAM and around 240 bytes of program memory.
 */
void fpsmon_draw(void);

#endif //CORE_FPSMON_H
