
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

#ifndef BOOT_INIT_H
#define BOOT_INIT_H

/**
 * Initialize game console system:
 * - Configure all registers to initialize all modules.
 * - Check battery status & level, sleep if battery too low.
 * This must be called to initialize the headless simulator.
 */
void sys_init(void);

/**
 * Called when device is about to go to sleep.
 * Peripherals are disabled.
 */
void sys_init_sleep(void);

/**
 * Called when device is waking up from sleep.
 * Peripherals are enabled.
 */
void sys_init_wakeup(void);

#endif //BOOT_INIT_H
