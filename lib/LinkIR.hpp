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
// - 4) Send NEC signals:
//       linkIR->sendNec(0x04, 0x03);
// - 5) Receive NEC signals:
//       // TODO: IMPLEMENT
// - 6) Send 38kHz signals:
//       linkIR->send({9000, 4500, 560, 560, 560, 1690, 0});
//                     // u16 array, numbers are microseconds
//                     // even indices: marks (IR on)
//                     // odd indices: spaces (IR off)
//                     // 0 = EOS
// - 7) Receive 38kHz signals;
//       u16 pulses[2000];
//       linkIR->receive(pulses, 2000, 30000);
//       // out array, max entries, timeout (in microseconds)
// - 8) Bitbang the LED manually:
//       linkIR->setLight(true);
// - 9) Receive manually:
//       bool ledOn = linkIR->isDetectingLight();
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "LinkGPIO.hpp"

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_IR_VERSION = "vLinkIR/v8.0.0";

#define LINK_IR_SIGNAL_END 0
#define LINK_IR_DEFAULT_PRIMARY_TIMER_ID 2
#define LINK_IR_DEFAULT_SECONDARY_TIMER_ID 3

/**
 * @brief A driver for the Infrared Adapter (AGB-006).
 * \warning To use this, make sure that `lib/iwram_code/LinkIR.cpp` gets
 * compiled! For example, in a Makefile-based project, verify that the directory
 * is in your `SRCDIRS` list.
 */
class LinkIR {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using Pin = LinkGPIO::Pin;
  using Direction = LinkGPIO::Direction;

  static constexpr int CYCLES_PER_MICROSECOND = 17;
  static constexpr int TIMEOUT_MICROSECONDS = 2500;
  static constexpr int DEMODULATION_38KHZ_PERIOD = 1000000 / 38000;
  static constexpr int DEMODULATION_WINDOW_FACTOR = 2;
  static constexpr int DEMODULATION_SAMPLE_WINDOW_CYCLES =
      DEMODULATION_38KHZ_PERIOD * DEMODULATION_WINDOW_FACTOR *
      CYCLES_PER_MICROSECOND;
  static constexpr int DEMODULATION_MIN_TRANSITIONS = 2;
  static constexpr int NEC_TOTAL_PULSES = 68;
  static constexpr int NEC_LEADER_MARK = 9000;
  static constexpr int NEC_LEADER_SPACE = 4500;
  static constexpr int NEC_PULSE = 560;
  static constexpr int NEC_SPACE_1 = 1690;
  static constexpr int NEC_SPACE_0 = 560;

 public:
  /**
   * @brief Constructs a new LinkIR object.
   * @param primaryTimerId `(0~3)` GBA Timer to use for counting time (1/2).
   * @param secondaryTimerId `(0~3)` GBA Timer to use for counting time (2/2).
   */
  explicit LinkIR(u8 primaryTimerId = LINK_IR_DEFAULT_PRIMARY_TIMER_ID,
                  u8 secondaryTimerId = LINK_IR_DEFAULT_SECONDARY_TIMER_ID) {
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

    return detected;
  }

  /**
   * Sends a NEC signal.
   * @param address An 8-bit address, to specify the device.
   * @param command An 8-bit command, to specify the action.
   */
  void sendNEC(u8 address, u8 command) {
    if (!isEnabled)
      return;

    u16 pulses[NEC_TOTAL_PULSES];
    u32 i = 0;

    pulses[i++] = NEC_LEADER_MARK;
    pulses[i++] = NEC_LEADER_SPACE;
    addNECByte(pulses, i, address);
    addNECByte(pulses, i, (u8)~address);
    addNECByte(pulses, i, command);
    addNECByte(pulses, i, (u8)~command);
    pulses[i++] = NEC_PULSE;
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
  void send(u16 pulses[]);  // defined in `LinkIR.cpp`

  /**
   * Receives a generic IR signal modulated at standard 38kHz.
   * @param pulses An array to be filled with u16 numbers describing the signal.
   * Even indices are *marks* (IR on), odd indices are *spaces* (IR off), and
   * `0` ends the signal.
   * @param maxEntries Maximum capacity of the `pulses` array.
   * @param timeout Number of microseconds inside a *space* after terminating
   * the reception. It doesn't start to count until the first *mark*.
   * @param startTimeout Number of microseconds before the first *mark* to abort
   * the reception.
   */
  bool receive(u16 pulses[],
               u32 maxEntries,
               u32 timeout,
               u32 startTimeout = 0xFFFFFFFF);  // defined in `LinkIR.cpp`

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
  bool isEmittingLight() { return linkGPIO.readPin(Pin::SO); }

  /**
   * Returns whether a remote light signal is detected through the `SI` pin
   * (LOW = DETECTED) or not.
   */
  bool isDetectingLight() { return !linkGPIO.readPin(Pin::SI); }

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

    detected = true;
  }

  struct Config {
    u8 primaryTimerId;
    u8 secondaryTimerId;
  };

  /**
   * @brief LinkIR configuration.
   * \warning `deactivate()` first, change the config, and `activate()` again!
   */
  Config config;

 private:
  LinkGPIO linkGPIO;
  volatile bool isEnabled = false;
  volatile bool detected = false;

  void addNECByte(u16 pulses[], u32& i, u8 value) {
    for (u32 b = 0; b < 8; b++) {
      pulses[i++] = NEC_PULSE;
      pulses[i++] = (value >> b) & 1 ? NEC_SPACE_1 : NEC_SPACE_0;
    }
  }

  void generate38kHzSignal(u32 microseconds);  // defined in ASM (`LinkIR.cpp`)
  void waitMicroseconds(u32 microseconds);     // defined in ASM (`LinkIR.cpp`)

  void resetState() { detected = false; }

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
