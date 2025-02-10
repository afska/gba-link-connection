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
//                     // u16 array, numbers are microseconds
//                     // even indices: marks (IR on)
//                     // odd indices: spaces (IR off)
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
#define LINK_IR_NEC_LEADER_MARK 9000
#define LINK_IR_NEC_LEADER_SPACE 4500
#define LINK_IR_NEC_PULSE 560
#define LINK_IR_NEC_SPACE_1 1690
#define LINK_IR_NEC_SPACE_0 560
#define LINK_IR_DEFAULT_TOLERANCE_PERCENTAGE 15
#define LINK_IR_DEFAULT_TIMER_ID 3

/**
 * @brief A driver for the Infrared Adapter (AGB-006).
 * \warning If you enable this, make sure that `LinkIR.cpp` gets compiled!
 * For example, in a Makefile-based project, verify that the file is in your
 * `SRCDIRS` list.
 */
class LinkIR {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using Pin = LinkGPIO::Pin;
  using Direction = LinkGPIO::Direction;

  static constexpr int TIMEOUT_MICROSECONDS = 2500;
  static constexpr int TO_TICKS = 17;

 public:
  /**
   * @brief Constructs a new LinkIR object.
   * @param tolerancePercentage Tolerance % for demodulation (default: 15).
   * @param modulationTimerId `(0~3)` GBA Timer to use for modulating
   * signals.
   * @param timerId `(0~3)` GBA Timer to use for counting elapsed time.
   */
  explicit LinkIR(u8 tolerancePercentage = LINK_IR_DEFAULT_TOLERANCE_PERCENTAGE,
                  u8 timerId = LINK_IR_DEFAULT_TIMER_ID) {
    config.tolerancePercentage = tolerancePercentage;
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
    waitMicroseconds(TIMEOUT_MICROSECONDS);
    linkGPIO.writePin(Pin::SO, false);
    linkGPIO.setSIInterrupts(false);

    bool success = irq;
    irq = false;

    return success;
  }

  void sendNEC(u8 address, u8 command) {
    u16 pulses[68];
    u32 i = 0;

    pulses[i++] = LINK_IR_NEC_LEADER_MARK;
    pulses[i++] = LINK_IR_NEC_LEADER_SPACE;
    addNECByte(pulses, i, address);
    addNECByte(pulses, i, (u8)~address);
    addNECByte(pulses, i, command);
    addNECByte(pulses, i, (u8)~command);
    pulses[i++] = LINK_IR_NEC_PULSE;
    pulses[i++] = LINK_IR_SIGNAL_END;

    send<true>(pulses);
  }

  /**
   * Sends a generic IR signal.
   * @param pulses An array of u16 numbers describing the signal. Even indices
   * are *marks* (IR on), odd indices are *spaces* (IR off), and `0` ends the
   * signal.
   * @tparam modulate38kHz Whether the *marks* should be modulated to 38kHz or
   * not. Most common protocols require this (for example, the NEC Protocol).
   */
  template <bool modulate38kHz>
  void send(u16* pulses) {
    linkGPIO.writePin(Pin::SO, false);
    for (u32 i = 0; pulses[i] != 0; i++) {
      u32 microseconds = pulses[i];
      bool isMark = i % 2 == 0;

      if (isMark) {
        if constexpr (modulate38kHz) {
          generate38kHzSignal(microseconds);
        } else {
          linkGPIO.writePin(Pin::SO, true);
          waitMicroseconds(microseconds);
        }
      } else {
        linkGPIO.writePin(Pin::SO, false);
        waitMicroseconds(microseconds);
      }
    }
    linkGPIO.writePin(Pin::SO, false);
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
    u8 tolerancePercentage;
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

  void addNECByte(u16* pulses, u32& i, u8 value) {
    for (u32 b = 0; b < 8; b++) {
      pulses[i++] = LINK_IR_NEC_PULSE;
      pulses[i++] =
          (value >> b) & 1 ? LINK_IR_NEC_SPACE_1 : LINK_IR_NEC_SPACE_0;
    }
  }

  void generate38kHzSignal(u32 microseconds);  // defined in ASM (`LinkIR.cpp`)
  void waitMicroseconds(u32 microseconds);     // defined in ASM (`LinkIR.cpp`)

  void resetState() { irq = false; }
};

extern LinkIR* linkIR;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_IR_ISR_SERIAL() {
  linkIR->_onSerial();
}

// TODO: C BINDINGS

#endif  // LINK_IR_H
