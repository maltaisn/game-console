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

INCLUDE common.ld

__TARGET_TEXT_LENGTH__ = __TEXT_REGION_LENGTH__ - BOOTLOADER_TEXT_SIZE;
__TARGET_DATA_LENGTH__ = __DATA_REGION_LENGTH__ - SHARED_DATA_SIZE;

MEMORY {
    flash  (rx) : ORIGIN = __TEXT_REGION_ORIGIN__, LENGTH = __TEXT_REGION_LENGTH__
    ram  (rw!x) : ORIGIN = __DATA_REGION_ORIGIN__, LENGTH = __DATA_REGION_LENGTH__
}

SECTIONS {
    /* ============ FLASH: BOOTLOADER TEXT ============ */

    /* Reserve space for bootloader text. */
    .boot_text : {
        . += BOOTLOADER_TEXT_SIZE;
    } > flash

    /* App text section. */
    .app_text : {
        __text_start = .;

        /* Callback vectors. */
        KEEP(*(.app.callback_loop));
        . = __text_start + 0x04;
        KEEP(*(.app.callback_draw));
        . = __text_start + 0x08;
        KEEP(*(.app.callback_sleep));
        . = __text_start + 0x0c;
        KEEP(*(.app.callback_wakeup));
        . = __text_start + 0x10;
        KEEP(*(.app.callback_sleep_scheduled));
        . = __text_start + 0x14;
        KEEP(*(.app.vector_uart_dre));
        . = __text_start + 0x18;
        KEEP(*(.app.vector_uart_rxc));
        . = __text_start + 0x1c;

        /* Setup callback. */
        KEEP(*(.init4));  /* Initialize data and BSS. Note that init4 is usually called in a
         startup context, but here we use it as a function body. This is fine because
         init4 only uses call-cloberred register (r0, r27, r28, r30, r31). */
        KEEP(*(.app.callback_setup));  /* Call the app setup. */

        /* Main code. */
        *(.text)
        . = ALIGN(2);
        *(.text.*)
        . = ALIGN(2);

        __text_end = .;
    } > flash

    /* App read-only data.  */
    .app_rodata ADDR(.app_text) + SIZEOF(.app_text) + __RODATA_PM_OFFSET__ : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata*)
        __rodata_end = .;
    } AT> flash

    /* ============ RAM: DATA SHARED BETWEEN BOOTLOADER AND APP ============ */

    .shared_data : {
        . += SHARED_DATA_SIZE;
    } > ram

    /* ============ RAM: DISPLAY BUFFER ============ */

    /* Display buffer (not initialized). The display buffer must be located at the
       same address in the bootloader, but it may differ in size. */
    .disp_buf (NOLOAD) : {
        __ram_start = .;
        __disp_buf_start = .;
        . = __disp_buf_start + DISPLAY_PAGE_HEIGHT * 64;
        __disp_buf_end = .;
    } > ram

    /* Temporary data sharing memory with display buffer (not initialized). */
    .shared_disp_buf ADDR(.disp_buf) (NOLOAD) : {
        __shared_disp_buf_start = .;
        *(.shared_disp_buf*)
        __shared_disp_buf_end = .;
    }

    /* ============ RAM: APP-ONLY DATA ============ */

    /* App data (ram vma / flash lma).  */
    .app_data MAX(__disp_buf_end, __shared_disp_buf_end) : {
        __data_start = .;
        *(.data)
        *(.data*)
        __data_end = .;
    } AT> flash
     __data_load_start = LOADADDR(.app_data);
     __data_load_end = __data_load_start + SIZEOF(.app_data);

    /* Global data cleared after reset.  */
    .app_bss ADDR(.app_data) + SIZEOF(.app_data) : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        __bss_end = .;
        __ram_end = .;
    }

    /* ============ RAM: STACK SPACE (not reserved) ============ */
}
