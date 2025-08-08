.section .text
.global _start
.global main

_start:
    /* Set up the stack pointer */
    LDR R0, =_estack
    MOV SP, R0

    /* Call the main function */
    BL main

    /* Infinite loop */
hang:
    B hang

.section .bss
    .space 0x200  /* Reserve space for the stack */ 

.section .data
    .global _estack
_estack:
    .word 0x20002000  /* Example stack top address */ 

.section .text
    .size _start, .- _start
    .size main, .- main