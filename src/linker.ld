ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")

SECTIONS
{
    . = 0x10000;

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata*)
    }

    .data :
    {
        *(.data)
    }

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss)
    }
}
