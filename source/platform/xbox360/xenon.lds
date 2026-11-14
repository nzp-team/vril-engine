/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 NZ:P Team */
ENTRY(_start)
PHDRS
{
    boot PT_LOAD FLAGS(7);
    game PT_LOAD FLAGS(7);
}
SECTIONS
{
    . = 0x80000000;
    .boot : { *(.except*); *(.pagetable); pagetable_end = .; *(.init*); *(.fini*); } :boot
    . = 0x81000000;
    .text : { *(.text*); } :game
    .elfldr : { elfldr_start = .; *(.elfldr); elfldr_end = .; }
    .data : { *(.data*); }
    .sdata : { *(.sdata*); }
    .rodata : { *(.rodata*); }
    .ctors : { __CTOR_LIST__ = .; *(.ctors); __CTOR_END__ = .; }
    .dtors : { __DTOR_LIST__ = .; *(.dtors); __DTOR_END__ = .; }
    .eh_frame_hdr : { *(.eh_frame_hdr); }
    .eh_frame ALIGN(0x4) : { PROVIDE(__eh_frame_start = .); KEEP(*(.eh_frame)); LONG(0); }
    .gcc_except_table : { *(.gcc_except_table); }
    bss_start = .;
    .bss : { *(.bss.beginning.lower); *(.bss.beginning.upper); *(.bss*); }
    .sbss : { *(.sbss .sbss.*); }
    bss_end = .;
    heap_begin = .;
    . = 0x9E000000;
    __libc_stack_end = .;
    heap_end = .;
    . = 0xA0000000;
}
