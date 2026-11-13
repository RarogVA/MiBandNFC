ENTRY(module_initialize)
SECTIONS {
  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .init_array : { KEEP(*(.init_array .init_array.*)) }
  .bss : { *(.bss .bss.* COMMON) }
}
