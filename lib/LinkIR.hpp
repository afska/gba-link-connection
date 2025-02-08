#ifndef LINK_IR_H
#define LINK_IR_H

// --------------------------------------------------------------------------
// A driver for the Infrared Adapter (AGB-006).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkIR* linkIR = new LinkIR();
// - 2) Add the interrupt service routines:
//       interrupt_init();
//       interrupt_add(INTR_SERIAL, LINK_IR_ISR_SERIAL);
//       interrupt_add(INTR_TIMER3, []() {});
// - 3) Initialize the library with:
//       linkIR->activate();
// - 4) Send IR signals:
//       linkIR->send({21, 2, 40, 4, 32, 0});
//                     // u16 array
//                     // even indices: marks (IR on)
//                     // odd indices: spaces (IR off)
//                     // numbers represent microseconds
//                     // 0 = EOS
// - 5) Receive IR signals;
//       u16 pulses[2000];
//       linkIR->receive(pulses, 2000, 30000);
//       // out array, max entries, timeout (in microseconds)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "LinkGPIO.hpp"

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_IR_VERSION = "vLinkIR/v8.0.0";

#define LINK_IR_SIGNAL_END 0
#define LINK_IR_DEFAULT_TIMER_ID 3

/**
 * @brief A driver for the Infrared Adapter (AGB-006).
 */
class LinkIR {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using Pin = LinkGPIO::Pin;
  using Direction = LinkGPIO::Direction;

  static constexpr int TIMEOUT_MICROSECONDS = 1000;
  static constexpr int TO_TICKS = 17;

 public:
  /**
   * @brief Constructs a new LinkIR object.
   * @param timerId `(0~3)` GBA Timer to use for waiting.
   */
  explicit LinkIR(u8 timerId = LINK_IR_DEFAULT_TIMER_ID) {
    config.timerId = timerId;
  }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library. Returns if the adapter is connected or not.
   */
  bool activate() {
    LINK_READ_TAG(LINK_IR_VERSION);

    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    resetState();
    linkGPIO.reset();

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;

    linkGPIO.setMode(Pin::SC, Direction::OUTPUT);
    linkGPIO.writePin(Pin::SC, false);
    linkGPIO.setMode(Pin::SO, Direction::OUTPUT);
    linkGPIO.writePin(Pin::SO, false);
    linkGPIO.setSIInterrupts(true);

    linkGPIO.writePin(Pin::SO, true);
    setTimer(TIMEOUT_MICROSECONDS);
    Link::_IntrWait(0,
                    Link::_IRQ_SERIAL | Link::_TIMER_IRQ_IDS[config.timerId]);

    bool success = irq;
    irq = false;
    return success;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    linkGPIO.reset();
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled)
      return;

    irq = true;
  }

  struct Config {
    u8 timerId;
  };

  /**
   * @brief LinkIR configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  LinkGPIO linkGPIO;
  volatile bool isEnabled = false;
  volatile bool irq = false;

  void resetState() { irq = false; }

  void setTimer(u32 microseconds) {
    u32 cycles = microseconds * TO_TICKS;
    Link::_REG_TM[config.timerId].start = -cycles;
    Link::_REG_TM[config.timerId].cnt =
        Link::_TM_ENABLE | Link::_TM_IRQ | Link::_TM_FREQ_1;
  }
};

extern LinkIR* linkIR;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_IR_ISR_SERIAL() {
  linkIR->_onSerial();
}

// TODO: IMPLEMENT
// TODO: C BINDINGS

#endif  // LINK_IR_H
