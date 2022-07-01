
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

#ifndef CORE_TRACE_H
#define CORE_TRACE_H

#if defined(SIMULATION) && !defined(RUNTIME_CHECKS)
// Always enable runtime checks in simulation.
#define RUNTIME_CHECKS
#endif

#ifdef SIMULATION

#include <stdio.h>

#define trace(str, ...) printf("GC %s:%d:(%s) " str "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define trace(str, ...) // no-op
#endif //SIMULATION

#endif //CORE_TRACE_H
