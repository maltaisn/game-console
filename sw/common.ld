
OUTPUT_FORMAT("elf32-avr","elf32-avr","elf32-avr")
OUTPUT_ARCH(avr:103)

__TEXT_REGION_ORIGIN__ = DEFINED(__TEXT_REGION_ORIGIN__) ? __TEXT_REGION_ORIGIN__ : 0;
__DATA_REGION_ORIGIN__ = DEFINED(__DATA_REGION_ORIGIN__) ? __DATA_REGION_ORIGIN__ : 0x803000;

__TEXT_REGION_LENGTH__ = DEFINED(__TEXT_REGION_LENGTH__) ? __TEXT_REGION_LENGTH__ : 32K;
__DATA_REGION_LENGTH__ = DEFINED(__DATA_REGION_LENGTH__) ? __DATA_REGION_LENGTH__ : 4K;

/* The start address of the flash in the memory map, for the .rodata VMA. */
__FLASH_OFFSET__ = DEFINED(__RODATA_PM_OFFSET__) ? __RODATA_PM_OFFSET__ : 0x4000;

/* The allocated program memory space for the bootloader.
   The BOOTEND fuse should be modified if this is changed. */
BOOTLOADER_TEXT_SIZE = 8192;

/* The allocated data size for the bootloader (excluding the display buffer). */
SHARED_DATA_SIZE = 160;
