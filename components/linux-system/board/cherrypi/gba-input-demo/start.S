/* Original GBA prototype demo. SPDX-License-Identifier: MIT */
.syntax unified
.cpu arm7tdmi
.arm
.section .text.start,"ax",%progbits
.global _start
_start:
    b start
    /* Header contains no Nintendo logo; this demo boots via gpSP direct ROM entry. */
    .space 0x9c
    .ascii "CHERRYPI DEM"
    .ascii "CPDE"
    .ascii "00"
    .byte 0x96, 0, 0
    .space 7
    .byte 0, 0
    .space 2
    .org 0xc0
start:
    mov r0, #0xd3
    msr cpsr_c, r0
    ldr sp, =0x03007f00
    bl demo_main
1:  b 1b
.section .note.GNU-stack,"",%progbits
