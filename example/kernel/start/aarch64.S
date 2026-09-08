.section .note.GNU-stack,"",%progbits

.section .bss
.balign 16
stack:
.skip 0x4000
stack_top:

.section .text.start
.global _start
_start:
  adrp x0, stack_top
  add x0, x0, :lo12:stack_top
  mov sp, x0
  bl kmain
1:
  b 1b
