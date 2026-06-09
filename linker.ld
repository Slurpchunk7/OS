ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS (R + X);
    rodata PT_LOAD FLAGS (R);
    data PT_LOAD FLAGS (R + W);
}

SECTIONS {
    . = 0xC8;
    .text   : { *(.text) } :text
    .rodata : { *(.rodata) } :rodata
    .data   : { *(.data) } :data
    .bss    : { *(.bss COMMON) } :data

    STACK_BASE = .;
    . += 0x40000000 + 8192;
    STACK_TOP = .;
}
