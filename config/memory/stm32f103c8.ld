/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 56K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) } > FLASH
  .hala_manifest : { . = ALIGN(4); KEEP(*(.hala_manifest)) } > FLASH
  .hala_dtb : { . = ALIGN(4); KEEP(*(.hala_dtb)) } > FLASH
  .hala_loader : { . = ALIGN(4); __hala_loader_start = .; KEEP(*(.hala_loader.text*)) __hala_loader_end = .; } > FLASH
  .hala_kernel : { . = ALIGN(4); __hala_kernel_start = .; KEEP(*(.hala_kernel.text*)) __hala_kernel_end = .; } > FLASH
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> FLASH
  /* Task stack được init_stack() ghi đầy đủ trước khi scheduler chạy, vì vậy
   * không cần zero ở Reset_Handler. Đặt section riêng giúp PSP không thay đổi
   * theo kích thước object pool và tránh tiến sát biên SRAM của STM32F103C8. */
  .task_stacks (NOLOAD) : { . = ALIGN(8); *(.task_stacks*) . = ALIGN(8); } > RAM
  .bss : { . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
  _end = .;
  .noinit.bootlog (NOLOAD) : { . = ALIGN(8); KEEP(*(.noinit.bootlog)) . = ALIGN(8); } > RAM
  .msp_stack (NOLOAD) :
  {
    . = ALIGN(8);
    __msp_stack_bottom = .;
    . += 2048;
    __msp_stack_top = .;
  } > RAM
  _estack = __msp_stack_top;
  __ram_end = ORIGIN(RAM) + LENGTH(RAM);
  __free_reserve_start = __msp_stack_top;
  __flash_image_end = LOADADDR(.data) + SIZEOF(.data);
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
ASSERT(__flash_image_end <= 0x0800E000, "HalaOS overlaps 8 KiB app store")
ASSERT(__msp_stack_top <= 0x20005000, "HalaOS exceeds 20 KiB SRAM")
ASSERT((__msp_stack_top - __msp_stack_bottom) >= 2048, "MSP reserve too small")
