
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

#ifndef BOOT_DEFS_H
#define BOOT_DEFS_H

#ifdef SIMULATION

#define BOOTLOADER_NOINLINE
#define BOOTLOADER_KEEP
#define BOOTLOADER_ONLY

#else

// Attribute put on functions used by bootloader so that they aren't inlined
// and can be called from the app instead of duplicating their code.
// The goal of all these attributes is to ensure the signature of the function stays unchanged.
#define BOOTLOADER_NOINLINE __attribute__((noinline)) __attribute__((noclone)) \
                            __attribute__((no_icf))

// Attribute put on variables only written but never read by the bootloader,
// so that they aren't removed and can be used from the app.
#define BOOTLOADER_KEEP __attribute__((used))

// Attribute put on variables used only by bootloader prior to app startup,
// so that they don't use up app RAM space. Note that these are *always* zero-initialized.
#define BOOTLOADER_ONLY __attribute__((section(".boot_only")))

#endif

#endif //BOOT_DEFS_H
