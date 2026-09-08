.section .note.GNU-stack,"",@progbits

.section .bss
.balign 16
stack:
.skip 0x4000
stack_top:

.section .text.start
.global _start
_start:
  lea stack_top(%rip), %rsp
  call kmain
1:
  jmp 1b
