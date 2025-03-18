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
//       linkIR->sendNEC(0x04, 0x03);
// - 5) Receive NEC signals:
//       u8 address, u8 command;
//       linkIR->receiveNEC(address, command);
// - 6) Send 38kHz signals:
//       linkIR->send({9000, 4500, 560, 560, 560, 1690, 0});
//                     // u16 array, numbers are microseconds
//                     // even indices: marks (IR on)
//                     // odd indices: spaces (IR off)
//                     // 0 = EOS
// - 7) Receive 38kHz signals:
//       u16 pulses[2000];
//       linkIR->receive(pulses, 2000);
//       // out array, max entries, timeout (in microseconds)
// - 8) Bitbang the LED manually:
//       linkIR->setLight(true);
// - 9) Receive manually:
//       bool ledOn = linkIR->isDetectingLight();
// --------------------------------------------------------------------------
// considerations:
// - wait at least 1 microsecond between `send(...)` and `receive(...)` calls!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "LinkGPIO.hpp"

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_IR_VERSION = "vLinkIR/v8.0.2";

#define LINK_IR_SIGNAL_END 0
#define LINK_IR_DEFAULT_PRIMARY_TIMER_ID 2
#define LINK_IR_DEFAULT_SECONDARY_TIMER_ID 3

/**
 * @brief A driver for the Infrared Adapter (AGB-006).
 * \warning To use this library, make sure that `lib/iwram_code/LinkIR.cpp` gets
 * compiled! For example, in a Makefile-based project, verify that the directory
 * is in your `SRCDIRS` list.
 */
class LinkIR {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu32 = Link::vu32;
  using Pin = LinkGPIO::Pin;
  using Direction = LinkGPIO::Direction;

  static constexpr int CYCLES_PER_MICROSECOND = 17;
  static constexpr int DETECTION_TIMEOUT = 1000;
  static constexpr int DEMODULATION_38KHZ_PERIOD = 1000000 / 38000;
  static constexpr int DEMODULATION_MARK_MIN_TRANSITIONS = 3;
  static constexpr int DEMODULATION_SPACE_PERIODS = 3;
  static constexpr int DEMODULATION_SPACE_THRESHOLD =
      DEMODULATION_38KHZ_PERIOD * DEMODULATION_SPACE_PERIODS;
  static constexpr int DEMODULATION_HYSTERESIS_DELAY = 10;
  static constexpr int NEC_TOLERANCE_PERCENTAGE = 15;
  static constexpr int NEC_TOTAL_PULSES = 68;
  static constexpr int NEC_LEADER_MARK = 9000;
  static constexpr int NEC_LEADER_SPACE = 4500;
  static constexpr int NEC_PULSE = 560;
  static constexpr int NEC_SPACE_1 = 1690;
  static constexpr int NEC_SPACE_0 = 560;
  static constexpr int DEFAULT_RECEIVE_TIMEOUT = 15000;
  static constexpr u32 NO_TIMEOUT = 0xFFFFFFFF;

 public:
  /**
   * @brief Constructs a new LinkIR object.
   * @param primaryTimerId `(0~3)` GBA Timer to use for measuring time (1/2).
   * @param secondaryTimerId `(0~3)` GBA Timer to use for measuring time (2/2).
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
   * @brief Activates the library. Returns whether the adapter is connected or
   * not.
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

    waitMicroseconds(DETECTION_TIMEOUT);
    setLight(true);
    waitMicroseconds(DETECTION_TIMEOUT);
    setLight(false);
    linkGPIO.setSIInterrupts(false);

    return detected;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    linkGPIO.reset();
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
   * Receives a signal and returns whether it's a NEC signal or not. If it is,
   * the `address` and `command` will be filled. Returns `true` on success.
   * @param address The read 8-bit address, specifying the device.
   * @param command The read 8-bit command, specifying the action.
   * @param startTimeout Number of microseconds before the first *mark* to abort
   * the reception.
   */
  bool receiveNEC(u8& address, u8& command, u32 startTimeout = NO_TIMEOUT) {
    if (!isEnabled)
      return false;

    u16 pulses[NEC_TOTAL_PULSES];
    if (!receive(pulses, NEC_TOTAL_PULSES, startTimeout))
      return false;

    return parseNEC(pulses, address, command);
  }

  /**
   * Tries to interpret an already received array of `pulses` as a NEC signal.
   * On success, returns `true` and fills the `address` and `command`
   * parameters.
   * @param pulses The pulses to interpret. Returns `true` on success.
   * @param address The read 8-bit address, specifying the device.
   * @param command The read 8-bit command, specifying the action.
   */
  bool parseNEC(u16 pulses[], u8& address, u8& command) {
    if (!isEnabled)
      return false;

    if (!isWithinNECTolerance(pulses[0], NEC_LEADER_MARK))
      return false;
    if (!isWithinNECTolerance(pulses[1], NEC_LEADER_SPACE))
      return false;

    u32 data = 0;
    for (u32 bit = 0; bit < 32; bit++) {
      u32 markIndex = 2 + bit * 2;
      u32 spaceIndex = markIndex + 1;
      if (!isWithinNECTolerance(pulses[markIndex], NEC_PULSE))
        return false;
      u16 space = pulses[spaceIndex];
      if (isWithinNECTolerance(space, NEC_SPACE_1))
        data |= 1 << bit;
      else if (!isWithinNECTolerance(space, NEC_SPACE_0))
        return false;
    }
    if (!isWithinNECTolerance(pulses[66], NEC_PULSE))
      return false;

    u8 addr = data & 0xFF;
    u8 invAddr = (data >> 8) & 0xFF;
    u8 cmd = (data >> 16) & 0xFF;
    u8 invCmd = (data >> 24) & 0xFF;
    if ((u8)~addr != invAddr || (u8)~cmd != invCmd)
      return false;

    address = addr;
    command = cmd;

    return true;
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
   * Receives a generic IR signal modulated at standard 38kHz. Returns whether
   * something was received or not.
   * @param pulses An array to be filled with u16 numbers describing the signal.
   * Even indices are *marks* (IR on), odd indices are *spaces* (IR off), and
   * `0` ends the signal.
   * @param maxEntries Maximum capacity of the `pulses` array.
   * @param startTimeout Number of microseconds before the first *mark* to abort
   * the reception.
   * @param signalTimeout Number of microseconds inside a *space* after
   * terminating the reception. It doesn't start to count until the first
   * *mark*.
   */
  bool receive(
      u16 pulses[],
      u32 maxEntries,
      u32 startTimeout = NO_TIMEOUT,
      u32 signalTimeout = DEFAULT_RECEIVE_TIMEOUT);  // defined in `LinkIR.cpp`

  /**
   * Turns the output IR LED ON/OFF through the `SO` pin (HIGH = ON).
   * @param on Whether the light should be ON.
   * \warning Add some pauses after every 10µs!
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
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial();  // defined in `LinkIR.cpp`

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
  vu32 firstLightTime = 0;
  vu32 lastLightTime = 0;
  vu32 transitionCount = 0;

  void addNECByte(u16 pulses[], u32& i, u8 value) {
    for (u32 b = 0; b < 8; b++) {
      pulses[i++] = NEC_PULSE;
      pulses[i++] = (value >> b) & 1 ? NEC_SPACE_1 : NEC_SPACE_0;
    }
  }

  bool isWithinNECTolerance(u16 measured, u16 expected) {
    if (measured == 0)
      return false;

    u32 tolerance = (expected * NEC_TOLERANCE_PERCENTAGE) / 100;
    return measured >= expected - tolerance && measured <= expected + tolerance;
  }

  void generate38kHzSignal(u32 microseconds);  // defined in ASM (`LinkIR.cpp`)
  void waitMicroseconds(u32 microseconds);     // defined in ASM (`LinkIR.cpp`)

  void resetState() {
    detected = false;
    firstLightTime = 0;
    lastLightTime = 0;
    transitionCount = 0;
  }

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

  LINK_INLINE u32 getCount() {
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

#endif  // LINK_IR_H
