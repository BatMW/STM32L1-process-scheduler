/*
 * GCC linker script for STM32 microcontrollers (ARM Cortex-M).
 *
 * It exports the symbols needed for the CMSIS assembler startup script for GCC
 * ARM toolchains (_sidata, _sdata, _edata, _sbss, _ebss) and sets the entry
 * point to Reset_Handler.
 *
 * Adapt FLASH/RAM size for your particular device below.
 *
 * @author Bjørn Forsman
 */

MEMORY
{
    flash (rx) : ORIGIN = 0x08000000, LENGTH = 256K
    ram  (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

ENTRY(Reset_Handler)

/*
 * Reserve memory for heap and stack. The linker will issue an error if there
 * is not enough memory.
 *
 * NOTE: The reserved heap and stack will be added to the bss column of the
 * binutils size command.
 */
_heap_size = 0;    /* required amount of heap  */
_stack_size = 1K;   /* required amount of stack for MSP*/

/* Stack block sizes */
STACK_BLOCK_SIZE_S = 128;
STACK_BLOCK_SIZE_M = 512;
STACK_BLOCK_SIZE_L = 1024;

/* Stack block counts */
STACK_BLOCK_COUNT_S = 20;
STACK_BLOCK_COUNT_M = 8;
STACK_BLOCK_COUNT_L = 4;

/* Stack pool sizes */
_stack_pool_s_size = STACK_BLOCK_SIZE_S * STACK_BLOCK_COUNT_S;
_stack_pool_m_size = STACK_BLOCK_SIZE_M * STACK_BLOCK_COUNT_M;
_stack_pool_l_size = STACK_BLOCK_SIZE_L * STACK_BLOCK_COUNT_L;

PROVIDE(__stack_block_size_s = STACK_BLOCK_SIZE_S);
PROVIDE(__stack_block_size_m = STACK_BLOCK_SIZE_M);
PROVIDE(__stack_block_size_l = STACK_BLOCK_SIZE_L);

PROVIDE(__stack_block_count_s = STACK_BLOCK_COUNT_S);
PROVIDE(__stack_block_count_m = STACK_BLOCK_COUNT_M);
PROVIDE(__stack_block_count_l = STACK_BLOCK_COUNT_L);

PROVIDE(__total_process_slots = STACK_BLOCK_COUNT_S + STACK_BLOCK_COUNT_M + STACK_BLOCK_COUNT_L);

ASSERT(((STACK_BLOCK_COUNT_S + STACK_BLOCK_COUNT_M + STACK_BLOCK_COUNT_L) &
       ((STACK_BLOCK_COUNT_S + STACK_BLOCK_COUNT_M + STACK_BLOCK_COUNT_L) - 1)) == 0,
       "Total number of stack blocks is not a power of 2");

/*
 * The stack starts at the end of RAM and grows downwards. Full-descending
 * stack; decrement first, then store.
 */
_estack = ORIGIN(ram) + LENGTH(ram);

SECTIONS
{
    /* Reset and ISR vectors */
    .isr_vector :
    {
        __isr_vector_start__ = .;
        KEEP(*(.isr_vector)) /* without 'KEEP' the garbage collector discards this section */
        ASSERT(. != __isr_vector_start__, "The .isr_vector section is empty");
    } >flash


    /* Text section (code and read-only data) */
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text*)   /* code */
        *(.rodata*) /* read only data */

        /*
         * NOTE: .glue_7 and .glue_7t sections are not needed because Cortex-M
         * only supports Thumb instructions, no ARM/Thumb interworking.
         */

        /* Static constructors and destructors */
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >flash


    /*
     * Stack unwinding and exception handling sections.
     *
     * ARM compilers emit object files with .ARM.extab and .ARM.exidx sections
     * when using C++ exceptions. Also, at least GCC emits those sections when
     * dividing large numbers (64-bit) in C. So we have to handle them.
     *
     * (ARM uses .ARM.extab and .ARM.exidx instead of the .eh_frame section
     * used on x86.)
     */
    .ARM.extab : /* exception unwinding information */
    {
        *(.ARM.extab*)
    } >flash
    .ARM.exidx : /* index entries for section unwinding */
    {
        *(.ARM.exidx*)
    } >flash


    /*
     * Newlib and Eglibc (at least) need these for C++ support.
     *
     * (Copied from Sourcery CodeBench Lite: arm-none-eabi-gcc -V)
     */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >flash
    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >flash
    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >flash


    /*
     * Initialized data section. This section is programmed into FLASH (LMA
     * address) and copied to RAM (VMA address) in startup code.
     */
    _sidata = .;
    .data : AT(_sidata) /* LMA address is _sidata (in FLASH) */
    {
        . = ALIGN(4);
        _sdata = .; /* data section VMA address */
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >ram


    /* Uninitialized data section (zeroed out by startup code) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >ram


    /*
     * Reserve memory for heap and stack. The linker will issue an error if
     * there is not enough memory.
     */
    ._heap (NOLOAD):
    {
        . = ALIGN(4);
        _heap_start = .;
        . = . + _heap_size;
        _heap_end = .;
        . = ALIGN(4);
    } >ram


.pool_s (NOLOAD) :
    {
        . = ALIGN(STACK_BLOCK_SIZE_S);
        _pool_s_start = .;
        . = . + _stack_pool_s_size;
        _pool_s_end = .;
    } >ram

    .pool_m (NOLOAD) :
    {
        . = ALIGN(STACK_BLOCK_SIZE_M);
        _pool_m_start = .;
        . = . + _stack_pool_m_size;
        _pool_m_end = .;
    } >ram

    .pool_l (NOLOAD) :
    {
        . = ALIGN(STACK_BLOCK_SIZE_L);
        _pool_l_start = .;
        . = . + _stack_pool_l_size;
        _pool_l_end = .;
    } >ram


    ._stack (NOLOAD):
    {
        . = ALIGN(4);
        . = . + _stack_size;
        . = ALIGN(4);
        _stack_end = .;
    } >ram

    _end = .;
}

ASSERT(_end <= ORIGIN(ram) + LENGTH(ram), "RAM overflow: data exceeds RAM size")
ASSERT(_sbss >= _edata, ".bss overlaps with .data — invalid layout")
ASSERT(_estack <= ORIGIN(ram) + LENGTH(ram), "MSP initial value (_estack) is outside RAM")


/* Nice to have */
PROVIDE(__stack_start = _estack);
__isr_vector_size__ = SIZEOF(.isr_vector);
__text_size__ = SIZEOF(.text);
__data_size__ = SIZEOF(.data);
__bss_size__ = SIZEOF(.bss);
