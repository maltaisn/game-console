
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

#ifndef BOOT_EEPROM_H
#define BOOT_EEPROM_H

/**
 * If the last write to EEPROM was not marked complete,
 * restore the old data from the write buffer. This is also atomic.
 * Note that this function must never be used when drawing because it uses the
 * display buffer as a temporary buffer for the atomic operation.
 */
void sys_eeprom_check_write(void);

#endif //BOOT_EEPROM_H
