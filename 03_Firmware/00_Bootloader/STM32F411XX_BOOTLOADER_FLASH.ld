/*
******************************************************************************
**  File        : STM32F411XX_BOOTLOADER_FLASH.ld
**  Target      : STM32F411CEUx
**
**  Abstract    : Linker script for STM32F411CEUx Bootloader
**                32KB FLASH (Bootloader), 128KB total RAM
**
**  Memory layout (matches 01_APP/STM32F411XX_FLASH.ld RTT placement so that
**  Bootloader and APP share the same RTT control-block address, otherwise
**  J-Link RTT Viewer would lock onto whichever image booted first):
**    FLASH   : 0x08000000 (32KB)  — Bootloader code + const
**    RAM     : 0x20000000 (121KB) — data, bss, heap, stack, NO_INIT
**    RTT_RAM : 0x2001E400 (7KB)   — SEGGER RTT control block + buffers
**                                   (must match 01_APP/STM32F411XX_FLASH.ld)
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400;     /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
RAM     (xrw) : ORIGIN = 0x20000000, LENGTH = 121K
RTT_RAM (xrw) : ORIGIN = 0x2001E400, LENGTH = 7K
FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)

    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* SEGGER RTT control block and channel buffers.  SEGGER_RTT_Conf.h overrides
     SEGGER_RTT_SECTION to ".RW_RTT" for the control block; the up/down ring
     buffers stay in the default "RTT_BUFFER" section (SEGGER_RTT.c hard-codes
     that name before our override has a chance to fan out).  Both names must
     be matched here so all RTT data lands at RTT_RAM (0x2001E400) and lines up
     with APP's layout — otherwise J-Link RTT Viewer locks onto whichever
     address it saw first and misses the other image's logs. */
  .RW_RTT (NOLOAD) :
  {
    . = ALIGN(4);
    _sRTT = .;
    *(.RW_RTT)
    *(.RW_RTT*)
    *(RTT_BUFFER)
    *(RTT_BUFFER*)
    . = ALIGN(4);
    _eRTT = .;
  } >RTT_RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
}
