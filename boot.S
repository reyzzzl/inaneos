# boot asm

.section .multiboot, "a"
.align 4
.long 0x1BADB002      # magic
.long 0x00000003      # flags: want mem info from bootloader
.long -(0x1BADB002 + 0x00000003)  # checksum, must sum to zero

.section .bss
.align 16
stack_bottom:
.skip 16384           # 16k stack
stack_top:

.section .text
.global _start
_start:
    # grub puts magic in eax and multiboot info ptr in ebx
    movl $stack_top, %esp
    pushl %ebx        # mbi ptr, 2nd arg
    pushl %eax        # magic, 1st arg
    call kernel_main

    # kernel_main returned
    cli
1:  hlt
    jmp 1b
