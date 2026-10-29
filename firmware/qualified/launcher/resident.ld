ENTRY(peaq_launcher_anchor_owner)
SECTIONS { .image 0x88fb7000 : { *(.text*) *(.rodata*) } .unexpected : { *(.data*) *(.bss*) *(COMMON) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) } }
ASSERT(SIZEOF(.image)<=4096,"V2 resident bundle exceeds candidate page")
ASSERT(SIZEOF(.unexpected)==0,"No writable firmware globals")
