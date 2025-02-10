#include "LinkIR.hpp"

// To modulate a signal to 38kHz, we need to stay 13.15µs LOW and 13.15µs HIGH.
//   38kHz signal => 38000/second =>
//     period = 1000000µs / 38000 = 26.31µs
//     halfPeriod = 13.15µs
// LED ON  => RCNT = 0x80BA (GPIO mode, SC, SD, SO as OUTPUT, SD=HIGH, SO=HIGH)
// LED OFF => RCNT = 0x80B2 (GPIO mode, SC, SD,  SO as OUTPUT, SD=HIGH, SO=LOW)

LINK_CODE_IWRAM void LinkIR::generate38kHzSignal(u32 microseconds) {
  // halfPeriods = ceil(microseconds / 13.15 µs) (in fixed-point math)
  u32 halfPeriods = Link::_max((microseconds * 100 + 1315) / 1316, 1);

  // the GBA is 16.776MHz => 13.15 µs ~= 220 cycles per half-period

  asm volatile(
      "mov    r0, %0          \n"  // r0 = address of REG_RCNT
      "ldr    r1, =0x80BA     \n"  // r1 = initial value 0x80BA (LED ON)
      "mov    r2, %1          \n"  // r2 = main loop count (halfPeriods)
      "1:                     \n"  // --- main loop ---
      "strh   r1, [r0]        \n"  // write current value to REG_RCNT
      "mov    r3, #54         \n"  // r3 = inner loop count (54) (*)
      "2:                     \n"  // --- inner loop ---
      "subs   r3, r3, #1      \n"  // decrement inner loop count
                                   // [1 cycle]
      "bne    2b              \n"  // repeat inner loop if needed
                                   // [taken: ~3 cycles, final: ~1 cycles]
      // (*) the we need to wait ~220 cycles between <main loop> iterations:
      //     [first 53 iterations (branch taken): 53 * ~4 cycles = ~212 cycles]
      //     [final iteration (branch not taken): ~2 cycles]
      //     [overhead: ~6 cycles]
      "eor    r1, r1, #8      \n"  // toggle r1: 0x80BA^8 = 0x80B2 (& viceversa)
      "subs   r2, r2, #1      \n"  // decrement main loop count
      "bne    1b              \n"  // repeat main loop if needed
      "ldr    r1, =0x80B2     \n"  // ensure we end with 0x80B2
      "strh   r1, [r0]        \n"  // write REG_RCNT = 0x80B2 (LED OFF)
      :
      : "r"(&Link::_REG_RCNT), "r"(halfPeriods)
      : "r0", "r1", "r2", "r3");
}

LINK_CODE_IWRAM void LinkIR::waitMicroseconds(u32 microseconds) {
  if (!microseconds)
    return;

  asm volatile(
      "mov    r1, %0          \n"  // r1 = main loop count (microseconds)
      "1:                     \n"  // --- main loop ---
      "mov    r2, #3          \n"  // r2 = inner loop count (3)
      "nop                    \n"  // extra cycle
      "nop                    \n"  // extra cycle
      "2:                     \n"  // --- inner loop ---
      "subs   r2, r2, #1      \n"  // decrement inner loop count
      "bne    2b              \n"  // repeat inner loop if needed
      "subs   r1, r1, #1      \n"  // decrement main loop count
      "bne    1b              \n"  // repeat main loop if needed
      :
      : "r"(microseconds)
      : "r1", "r2");
}
