/**********************************/
/* NINTENDO E-READER STARTUP CODE */
/**********************************/
/* Author  : Tim Schuerewegen     */
/* Version : 1.0                  */
/**********************************/

  .GLOBAL _start

  .TEXT

  .ARM

_start:

  @ enter thumb mode
  LDR  R0, =(_start_thumb+1)
  BX   R0

  @ For some reason the usa e-reader subtracts 0x0001610C from the value at
  @ address 0x02000008 if it is not "valid". This is only the case when
  @ running as dot code, not when running from flash. However, it is
  @ recommended to put a "valid" value at that address because the jap
  @ e-reader does not have this kind of "protection".
  @ 0x02000000 <= valid value < 0x020000E4
  .POOL

  .THUMB

_start_thumb:

  @ save return address
  PUSH  {LR}

  @ clear bss section
_bss_clear:
  LDR  R0, =__bss_start
  LDR  R1, =__bss_end
  MOV  R2, #0
_bss_clear_loop:
  CMP  R0, R1
  BEQ  _bss_clear_exit
  STRB R2, [R0]
  ADD  R0, #1
  B    _bss_clear_loop
_bss_clear_exit:

  @ restore return address
  POP  {R3}
  MOV  LR, R3

  @ jump to main
  LDR  R3, =main
  BX   R3

  .ALIGN

  .POOL

  .END
