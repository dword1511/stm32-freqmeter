/* STM32F103x8, 64K flash, 20K RAM, bootloader will be erased */

/* Define memory regions. */
MEMORY {
  rom (rx ) : ORIGIN = 0x08000000, LENGTH = 64K
  ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Include the common ld script. */
INCLUDE libopencm3_stm32f1.ld
