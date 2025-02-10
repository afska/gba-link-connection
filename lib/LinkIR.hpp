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
//       interrupt_add(INTR_TIMER2, []() {});
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
#define LINK_38KHZ_PERIOD (1000000 / 38000)
#define LINK_IR_NEC_TOTAL_PULSES 68
#define LINK_IR_NEC_LEADER_MARK 9000
#define LINK_IR_NEC_LEADER_SPACE 4500
#define LINK_IR_NEC_PULSE 560
#define LINK_IR_NEC_SPACE_1 1690
#define LINK_IR_NEC_SPACE_0 560
#define LINK_IR_DEFAULT_TOLERANCE_PERCENTAGE 15
#define LINK_IR_DEFAULT_PRIMARY_TIMER_ID 2
#define LINK_IR_DEFAULT_SECONDARY_TIMER_ID 3

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
  static constexpr int TO_CYCLES = 17;

 public:
  /**
   * @brief Constructs a new LinkIR object.
   * @param tolerancePercentage Tolerance % for demodulation (default: 15).
   * @param modulationTimerId `(0~3)` GBA Timer to use for modulating
   * signals.
   * @param timerId `(0~3)` GBA Timer to use for counting elapsed time.
   */
  explicit LinkIR(u8 tolerancePercentage = LINK_IR_DEFAULT_TOLERANCE_PERCENTAGE,
                  u8 primaryTimerId = LINK_IR_DEFAULT_PRIMARY_TIMER_ID,
                  u8 secondaryTimerId = LINK_IR_DEFAULT_SECONDARY_TIMER_ID) {
    config.tolerancePercentage = tolerancePercentage;
    config.primaryTimerId = primaryTimerId;
    config.secondaryTimerId = secondaryTimerId;
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
    linkGPIO.setMode(Pin::SD, Direction::OUTPUT);
    linkGPIO.writePin(Pin::SD, true);
    linkGPIO.setMode(Pin::SO, Direction::OUTPUT);
    linkGPIO.writePin(Pin::SO, false);
    linkGPIO.setSIInterrupts(true);

    setLight(true);
    waitMicroseconds(TIMEOUT_MICROSECONDS);
    setLight(false);
    linkGPIO.setSIInterrupts(false);

    bool success = irq;
    irq = false;

    return success;
  }

  void sendNEC(u8 address, u8 command) {
    u16 pulses[LINK_IR_NEC_TOTAL_PULSES];
    u32 i = 0;

    pulses[i++] = LINK_IR_NEC_LEADER_MARK;
    pulses[i++] = LINK_IR_NEC_LEADER_SPACE;
    addNECByte(pulses, i, address);
    addNECByte(pulses, i, (u8)~address);
    addNECByte(pulses, i, command);
    addNECByte(pulses, i, (u8)~command);
    pulses[i++] = LINK_IR_NEC_PULSE;
    pulses[i++] = LINK_IR_SIGNAL_END;

    send(pulses);
  }

  /**
   * Sends a generic IR signal, modulating at standard 38kHz.
   * @param pulses An array of u16 numbers describing the signal. Even indices
   * are *marks* (IR on), odd indices are *spaces* (IR off), and `0` ends the
   * signal.
   * \warning The carrier frequency is tied to the ASM code. To transmit in
   * other frequencies, you'll have to bitbang the `SO` pin yourself with
   * `setLight(...)`.
   */
  void send(u16* pulses) {
    setLight(false);

    for (u32 i = 0; pulses[i] != 0; i++) {
      u32 microseconds = pulses[i];
      bool isMark = i % 2 == 0;

      if (isMark) {
        generate38kHzSignal(microseconds);
      } else {
        setLight(false);
        waitMicroseconds(microseconds);
      }
    }
  }

  bool receiveNEC(u8& address, u8& command) {
    // TODO: IMPLEMENT
    return false;
  }

  bool receive(u16 pulses[], u32 maxEntries, u32 timeout);  // IWRAM

  /**
   * Turns the output IR LED ON/OFF through the `SO` pin (HIGH = ON).
   * @param on Whether the light should be ON.
   * \warning The adapter won't keep it ON for more than 10µs. Add some pauses
   * after every 10µs.
   */
  void setLight(bool on) { linkGPIO.writePin(Pin::SO, on); }

  /**
   * Returns whether the output IR LED is ON or OFF.
   */
  bool getOutputLight() {
    return linkGPIO.readPin(Pin::SO);
  }  // TODO: isEmittingLight()

  /**
   * Returns whether a remote light signal is detected through the `SI` pin
   * (LOW = DETECTED) or not.
   */
  bool getInputLight() {
    return !linkGPIO.readPin(Pin::SI);
  }  // TODO: isDetectingLight()

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
    u8 primaryTimerId;
    u8 secondaryTimerId;
  };

  /**
   * @brief LinkIR configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

  //  private: // TODO: REMOVE
  LinkGPIO linkGPIO;
  volatile bool isEnabled = false;
  volatile bool irq = false;

  void addNECByte(u16 pulses[], u32& i, u8 value) {
    for (u32 b = 0; b < 8; b++) {
      pulses[i++] = LINK_IR_NEC_PULSE;
      pulses[i++] =
          (value >> b) & 1 ? LINK_IR_NEC_SPACE_1 : LINK_IR_NEC_SPACE_0;
    }
  }

  void generate38kHzSignal(u32 microseconds);  // defined in ASM (`LinkIR.cpp`)
  void waitMicroseconds(u32 microseconds);     // defined in ASM (`LinkIR.cpp`)

  void resetState() { irq = false; }

  void startCount() {
    Link::_REG_TM[config.primaryTimerId].start = 0;
    Link::_REG_TM[config.secondaryTimerId].start = 0;

    Link::_REG_TM[config.primaryTimerId].cnt = 0;
    Link::_REG_TM[config.secondaryTimerId].cnt = 0;

    Link::_REG_TM[config.secondaryTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_CASCADE;
    Link::_REG_TM[config.primaryTimerId].cnt =
        Link::_TM_ENABLE | Link::_TM_FREQ_1;
  }

  u32 getCount() {
    return (Link::_REG_TM[config.primaryTimerId].count |
            (Link::_REG_TM[config.secondaryTimerId].count << 16));
  }

  u32 stopCount() {
    Link::_REG_TM[config.primaryTimerId].cnt = 0;
    Link::_REG_TM[config.secondaryTimerId].cnt = 0;

    return getCount();
  }
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
