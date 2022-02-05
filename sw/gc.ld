/*
 * Adapted from avrxmega3.xn default linker script:
 * - Cleanup including removal of C++ sections, debug sections and other unused sections.
 * - Change memory sections to declare only those used (flash, ram).
 * - Add section for display buffer and temporary data stored in same memory as display buffer.
 */

OUTPUT_FORMAT("elf32-avr","elf32-avr","elf32-avr")
OUTPUT_ARCH(avr:103)

__TEXT_REGION_ORIGIN__ = DEFINED(__TEXT_REGION_ORIGIN__) ? __TEXT_REGION_ORIGIN__ : 0;
__DATA_REGION_ORIGIN__ = DEFINED(__DATA_REGION_ORIGIN__) ? __DATA_REGION_ORIGIN__ : 0x802000;
__TEXT_REGION_LENGTH__ = DEFINED(__TEXT_REGION_LENGTH__) ? __TEXT_REGION_LENGTH__ : 1024K;
__DATA_REGION_LENGTH__ = DEFINED(__DATA_REGION_LENGTH__) ? __DATA_REGION_LENGTH__ : 0xffa0;
__RODATA_PM_OFFSET__ = DEFINED(__RODATA_PM_OFFSET__) ? __RODATA_PM_OFFSET__ : 0x8000;

MEMORY {
    flash  (rx) : ORIGIN = __TEXT_REGION_ORIGIN__, LENGTH = __TEXT_REGION_LENGTH__
    ram  (rw!x) : ORIGIN = __DATA_REGION_ORIGIN__, LENGTH = __DATA_REGION_LENGTH__
}

SECTIONS {
    /* Internal text space or external memory. */
    .text : {
        __text_start = .;

        /* Interrupt vectors table */
        *(.vectors)
        KEEP(*(.vectors))

        /* Initialization sections */
        *(.init0)  /* Start here after reset. */
        KEEP(*(.init0))
        *(.init2)  /* Clear __zero_reg__, set up stack pointer. */
        KEEP(*(.init2))
        *(.init4)  /* Initialize data and BSS. */
        KEEP(*(.init4))
        *(.init9)  /* Call main(). */
        KEEP(*(.init9))

        /* Main code. */
        *(.text)
        . = ALIGN(2);
        *(.text.*)
        . = ALIGN(2);

        /* Finalization sections */
        *(.fini0)  /* Infinite loop after program termination.  */
        KEEP(*(.fini0))

        _etext = .;
        __text_end = .;
    } > flash

    /* Global read-only data.  */
    .rodata ADDR(.text) + SIZEOF(.text) + __RODATA_PM_OFFSET__ : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata*)
        __rodata_end = .;
    } AT> flash

    /* Global data initialized from program memory.  */
    .data : {
        __data_start = .;
        *(.data)
        *(.data*)
        . = ALIGN(2);
        _edata = . ;
        __data_end = .;
    } > ram AT> flash

     __data_load_start = LOADADDR(.data);
     __data_load_end = __data_load_start + SIZEOF(.data);

    /* Global data cleared after reset.  */
    .bss ADDR(.data) + SIZEOF(.data) : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > ram

    /* Global data not cleared after reset.  */
    .noinit ADDR(.bss) + SIZEOF(.bss) : {
        __noinit_start = .;
        *(.noinit*)
        __noinit_end = .;
    } > ram

    /* Display buffer (not initialized).  */
    .disp_buf ADDR(.noinit) + SIZEOF(.noinit) (NOLOAD) : {
        __disp_buf_start = .;
        *(.disp_buf*)
        __disp_buf_end = .;
    } > ram

    /* Temporary data sharing memory with display buffer (not initialized). */
    .shared_disp_buf ADDR(.disp_buf) (NOLOAD) : {
        __shared_disp_buf_start = .;
        *(.shared_disp_buf*)
        __shared_disp_buf_end = .;
    } > ram

    /* End address depends on the size of the largest of disp_buf and shared_disp_buf sections. */
    _end = __disp_buf_end > __shared_disp_buf_end ? __disp_buf_end : __shared_disp_buf_end;
}
