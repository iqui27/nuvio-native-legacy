SECTIONS {
  . = 0x01a42340;
  .blob : { *(.text .text.*) *(.rodata .rodata.*) }
  /DISCARD/ : { *(*) }
}
