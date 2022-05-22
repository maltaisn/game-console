
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

#include <sys/eeprom.h>

void eeprom_read(eeprom_t address, uint8_t length, void* dest) {
    sys_eeprom_read_relative(address, length, dest);
}

void eeprom_write(eeprom_t address, uint8_t length, const void* src) {
    sys_eeprom_write_relative(address, length, src);
}
