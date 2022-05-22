
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

#include <core/display.h>
#include <sys/display.h>

void display_set_inverted(bool inverted) {
    sys_display_set_inverted(inverted);
}

void display_set_contrast(uint8_t contrast) {
    sys_display_set_contrast(contrast);
}

uint8_t display_get_contrast(void) {
    return sys_display_get_contrast();
}
