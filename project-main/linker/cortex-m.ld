MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 128K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 20K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))  /* Interrupt vector table */
        *(.text*)             /* All .text sections from all files */
        *(.rodata*)           /* Read-only data */
    } > FLASH

    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        *(.data*)             /* All .data sections from all files */
    } > RAM

    .bss :
    {
        *(.bss*)              /* All .bss sections from all files */
        *(COMMON)             /* Common symbols */
    } > RAM

    _estack = ORIGIN(RAM) + LENGTH(RAM); /* End of stack */
    _sdata = .;                          /* Start of .data section */
    _edata = .;                          /* End of .data section */
    _sbss = .;                           /* Start of .bss section */
    _ebss = .;                           /* End of .bss section */
}