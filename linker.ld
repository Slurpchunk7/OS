ENTRY(_start)

PHDRS {
    text   PT_LOAD FLAGS (5);
    rodata PT_LOAD FLAGS (4);
    data   PT_LOAD FLAGS (6);
}

SECTIONS {
    . = 0x42000000;

    .text   : { *(.text*)   } :text
    .rodata : { *(.rodata*) } :rodata   
    .data   : { *(.data*)   } :data
    .bss    : {
        __bss_start = .;
        *(.bss .bss* COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(16);
    STACK_BASE = .;
    . += 8192;
    STACK_TOP = .;
}