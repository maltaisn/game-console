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

#include <sys/reset.h>

#include <core/trace.h>

#include <stdlib.h>

void sys_reset_system(void) {
    // reset is used to crash the app and go back to bootloader.
    // there is no bootloader in simulator so we just terminate the process.
    trace("system reset, exiting.");
    exit(EXIT_FAILURE);
}
