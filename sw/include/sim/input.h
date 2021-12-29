
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

#ifndef SIM_INPUT_H
#define SIM_INPUT_H

void input_on_key_down(unsigned char key, int x, int y);

void input_on_key_up(unsigned char key, int x, int y);

void input_on_key_down_special(int key, int x, int y);

void input_on_key_up_special(int key, int x, int y);

#endif //SIM_INPUT_H
