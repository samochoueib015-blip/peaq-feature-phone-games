ENTRY(menu_kernel_bridge)
SECTIONS {
 .bridge 0x81579e00 : { *(.bridge) *(.text*) *(.rodata*) . = ALIGN(4); }
 .unexpected : { *(.data*) *(.bss*) *(COMMON) }
 ASSERT(SIZEOF(.unexpected)==0,"gate must have no mutable global state")
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
