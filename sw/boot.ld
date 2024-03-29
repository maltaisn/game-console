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

__FUSE_REGION_ORIGIN__ = 0x820000;
__FUSE_REGION_LENGTH__ = DEFINED(__FUSE_REGION_LENGTH__) ? __FUSE_REGION_LENGTH__ : 1K;

__TEXT_REGION_LENGTH__ = BOOTLOADER_TEXT_SIZE;

__TARGET_TEXT_LENGTH__ = BOOTLOADER_TEXT_SIZE;
__TARGET_DATA_LENGTH__ = SHARED_DATA_SIZE;

/* The start address at which the app code will be copied to in program memory,
   in global memory map. */
__APP_START_ADDRESS = __FLASH_OFFSET__ + __TEXT_REGION_ORIGIN__ + BOOTLOADER_TEXT_SIZE;

MEMORY {
    flash  (rx) : ORIGIN = __TEXT_REGION_ORIGIN__, LENGTH = __TEXT_REGION_LENGTH__
    ram  (rw!x) : ORIGIN = __DATA_REGION_ORIGIN__, LENGTH = __DATA_REGION_LENGTH__
    fuse (rw!x) : ORIGIN = __FUSE_REGION_ORIGIN__, LENGTH = __FUSE_REGION_LENGTH__
}

SECTIONS {
    .fuse : {
        KEEP(*(.fuse))
    } > fuse

    /* ============ FLASH: BOOTLOADER TEXT ============ */

    /* Bootloader text section. */
    .boot_text : {
        __text_start = .;

        /* Interrupt vectors table */
        KEEP(*(.vectors))

        /* Initialization sections */
        KEEP(*(.init0))  /* Start here after reset. */
        KEEP(*(.init2))  /* Clear __zero_reg__, set up stack pointer. */
        KEEP(*(.init4))  /* Initialize data and BSS. */
        KEEP(*(.init9))  /* Call main(). */

        /* Main code. */
        *(.text*)
        . = ALIGN(2);

        /* Finalization sections */
        KEEP(*(.fini0)) /* Infinite loop after program termination.  */

        __text_end = .;
    } > flash

    /* Bootloader read-only data.  */
    .boot_rodata ADDR(.boot_text) + SIZEOF(.boot_text) + __FLASH_OFFSET__ : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata*)
        __rodata_end = .;
    } AT> flash

    /* ============ RAM: DATA SHARED BETWEEN BOOTLOADER AND APP ============ */

    /* Shared data (ram vma / flash lma).  */
    .shared_data (NOLOAD) : {
        __ram_start = .;
        __data_start = .;
        *(.data)
        *(.data*)
        __data_end = .;
    } > ram AT> flash
    __data_load_start = LOADADDR(.shared_data);
    __data_load_end = __data_load_start + SIZEOF(.shared_data);

    /* Global data cleared after reset.  */
    .shared_bss (NOLOAD) : {
        __bss_start = .;
        __bss_proper_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        /* Shared data ends here. */
        __bss_proper_end = .;
        __ram_end = .;
    } > ram

    /* ============ RAM: DISPLAY BUFFER ============ */

    /* Display buffer (zero initialized). The display buffer must be located at the
       same address in the app, but it may differ in size. */
    .disp_buf ORIGIN(ram) + SHARED_DATA_SIZE (NOLOAD) : {
        __disp_buf_start = .;
        *(.disp_buf*)
        . = __disp_buf_start + DISPLAY_PAGE_HEIGHT * 64;
        __disp_buf_end = .;
    }

    /* Temporary data sharing memory with display buffer (not initialized). */
    .shared_disp_buf ADDR(.disp_buf) (NOLOAD) : {
        __shared_disp_buf_start = .;
        *(.shared_disp_buf*)
        __shared_disp_buf_end = .;
    }

    /* ============ RAM: BOOTLOADER-ONLY DATA ============ */

    /* Data in this section cannot be accessed after the app has been started. */
    .boot_bss MAX(__disp_buf_end, __shared_disp_buf_end) (NOLOAD) : {
        __boot_only_start = .;
        *(.boot_only*)
        __boot_only_end = .;
        __bss_end = .;
    }

    /* ============ RAM: STACK SPACE (not reserved) ============ */
}
