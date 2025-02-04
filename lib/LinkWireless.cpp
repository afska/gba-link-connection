#include "LinkWireless.hpp"

#if defined(LINK_WIRELESS_PUT_ISR_IN_IWRAM) || \
    defined(LINK_WIRELESS_ENABLE_NESTED_IRQ)

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM

#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL == 1
#define _LINK_SERIAL_ISR \
  LINK_CODE_IWRAM        \
  __attribute__((optimize(LINK_WIRELESS_PUT_ISR_IN_IWRAM_SERIAL_LEVEL)))
#else
#define _LINK_SERIAL_ISR
#endif

#if LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER == 1
#define _LINK_TIMER_ISR \
  LINK_CODE_IWRAM       \
  __attribute__((optimize(LINK_WIRELESS_PUT_ISR_IN_IWRAM_TIMER_LEVEL)))
#else
#define _LINK_TIMER_ISR
#endif

#endif

_LINK_SERIAL_ISR void LinkWireless::_onSerial() {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  interrupt = true;
  LINK_BARRIER;
  // (nested interrupts are enabled by LinkRawWireless::_onSerial(...))
#endif

  __onSerial();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  irqEnd();
#endif
}

_LINK_TIMER_ISR void LinkWireless::_onTimer() {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  if (interrupt)
    return;

  interrupt = true;
  LINK_BARRIER;
  Link::_REG_IME = 1;
#endif

  __onTimer();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  irqEnd();
#endif
}

/**
 * NOTES:
 * When using `LINK_WIRELESS_ENABLE_NESTED_IRQ`:
 *   - Any user ISR can interrupt the library ISRs.
 *   - SERIAL ISR can interrupt TIMER ISR.
 *     -> This doesn't cause data races since TIMER ISR only works when
 *        there is no active async task.
 *     -> When TIMER ISR starts an async task (`transferAsync(...)`),
 *        nested interrupts are disabled (`REG_IME = 0`) and SERIAL cannot
 *        interrupt anymore.
 *   - TIMER interrupts are skipped if SERIAL ISR is running.
 *   - VBLANK interrupts are postponed if SERIAL or TIMER ISRs are running.
 *   - Nobody can interrupt VBLANK ISR.
 */

#endif
