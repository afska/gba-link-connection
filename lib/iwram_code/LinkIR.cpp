#include "../LinkIR.hpp"

LINK_CODE_IWRAM void LinkIR::send(u16 pulses[]) {
  setLight(false);

  for (u32 i = 0; pulses[i] != 0; i++) {
    u32 microseconds = pulses[i];
    bool isMark = i % 2 == 0;

    if (isMark) {
      // even index: mark
      generate38kHzSignal(microseconds);
    } else {
      // odd index: space
      setLight(false);
      waitMicroseconds(microseconds);
    }
  }
}

LINK_CODE_IWRAM bool LinkIR::receive(u16 pulses[],
                                     u32 maxEntries,
                                     u32 timeout,
                                     u32 startTimeout) {
  bool hasStarted = false;
  bool isMark = false;

  u32 pulseIndex = 0;
  u32 lastTransitionTime = 0;
  u32 initialTime = 0;

  startCount();
  initialTime = getCount();

  while (true) {
    // begin a fixed demodulation window
    u32 windowStart = getCount();
    u32 transitionsCount = 0;
    bool previousRaw = isDetectingLight();

    // sample for a fixed window duration
    while (getCount() - windowStart < DEMODULATION_SAMPLE_WINDOW_CYCLES) {
      bool currentRaw = isDetectingLight();
      if (currentRaw != previousRaw) {
        transitionsCount++;
        previousRaw = currentRaw;
      }
    }

    bool isCarrierPresent = transitionsCount >= DEMODULATION_MIN_TRANSITIONS;

    // new transition?
    if (isCarrierPresent != isMark) {
      // estimate transition time as the middle of the current window
      u32 estimatedNow = windowStart + DEMODULATION_SAMPLE_WINDOW_CYCLES / 2;
      if (!hasStarted && isCarrierPresent) {
        // first mark initializes the capture
        hasStarted = true;
        lastTransitionTime = estimatedNow;
      } else if (hasStarted) {
        // record the pulse duration in microseconds
        if (pulseIndex >= maxEntries - 1)
          break;
        u32 pulseDuration =
            (estimatedNow - lastTransitionTime) / CYCLES_PER_MICROSECOND;
        pulses[pulseIndex++] = pulseDuration;
        lastTransitionTime = estimatedNow;
      }
      isMark = isCarrierPresent;
    }

    // if we've started and we're in a space, check for overall timeout
    if (hasStarted && !isMark &&
        (getCount() - lastTransitionTime) / CYCLES_PER_MICROSECOND >= timeout)
      break;

    // if we haven't started and we've waited longer than startTimeout, then
    // timeout too
    if (!hasStarted &&
        (getCount() - initialTime) / CYCLES_PER_MICROSECOND >= startTimeout)
      break;
  }

  pulses[pulseIndex] = LINK_IR_SIGNAL_END;
  stopCount();
  return pulseIndex > 0;
}

/**
 * NOTES:
 * To modulate a signal at 38kHz, we need to stay 13.15µs LOW and 13.15µs HIGH.
 *   38kHz signal => 38000/second =>
 *     period = 1000000µs / 38000 = 26.31µs
 *     halfPeriod = 13.15µs
 * LED ON  => RCNT = 0x80BA (GPIO mode, SC, SD, SO as OUTPUT, SD=HIGH, SO=HIGH)
 * LED OFF => RCNT = 0x80B2 (GPIO mode, SC, SD,  SO as OUTPUT, SD=HIGH, SO=LOW)
 */

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
