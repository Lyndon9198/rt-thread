OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_reset)

SECTIONS
{
    . = 0x00200000;

    __text_start = .;
    .text :
    {
        KEEP(*(.boot))
        . = ALIGN(32);
        KEEP(*(.vectors))
        *(.text)
        *(.text.*)

        . = ALIGN(4);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;

        . = ALIGN(4);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;

        . = ALIGN(4);
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;
    }
    __text_end = .;

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    }

    . = ALIGN(4);
    __rodata_start = .;
    .rodata : { *(.rodata) *(.rodata.*) }
    __rodata_end = .;

    . = ALIGN(8);
    .ctors :
    {
        PROVIDE(__ctors_start__ = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE(__ctors_end__ = .);
    }

    .dtors :
    {
        PROVIDE(__dtors_start__ = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE(__dtors_end__ = .);
    }

    . = ALIGN(8);
    __data_start = .;
    .data : { *(.data) *(.data.*) }
    __data_end = .;

    . = ALIGN(8);
    __bss_start = .;
    .bss (NOLOAD) :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(8);
    }
    __bss_end = .;
    _end = .;

    /DISCARD/ : { *(.note.GNU-stack) }
}
