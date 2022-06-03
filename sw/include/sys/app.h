
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

#ifndef SYS_APP_H
#define SYS_APP_H

#include <stdint.h>

// This app ID is used to identify the bootloader.
// It's also used to identify when no app is loaded.
#define SYS_APP_ID_NONE 0

extern uint8_t sys_loaded_app_id;

/**
 * Get the ID of the currently loaded app, or `SYS_APP_ID_NONE` if the bootloader is active.
 * This function is implemented in `core/app.c`.
 */
uint8_t sys_app_get_loaded_id(void);

#endif //SYS_APP_H
