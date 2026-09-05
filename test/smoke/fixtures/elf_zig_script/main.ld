ENTRY(_start)
SECTIONS {
  . = 0x400000;
  .text : { *(.text.start) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .note : { *(.note .note.*) }
  .eh_frame_hdr : { *(.eh_frame_hdr) }
  .eh_frame : { *(.eh_frame) }
  .data : { *(.data .data.*) }
  .bss : { *(.bss .bss.*) }
}
