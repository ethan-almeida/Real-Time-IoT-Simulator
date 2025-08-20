MEMORY
{
    RAM (rwx) : ORIGIN = 0x00008000, LENGTH = 256M
}

ENTRY(_start)

SECTIONS
{
    . = 0x00008000;
    
    .text : {
        *(.text.boot)
        *(.text*)
        *(.rodata*)
    } > RAM
    
    .data : {
        *(.data*)
    } > RAM
    
    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM
}