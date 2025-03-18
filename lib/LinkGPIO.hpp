#ifndef LINK_GPIO_H
#define LINK_GPIO_H

// --------------------------------------------------------------------------
// A General Purpose Input-Output handler for the Link Port.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkGPIO* linkGPIO = new LinkGPIO();
// - 2) Initialize the library with:
//       linkGPIO->reset();
// - 3) Write pins by using:
//       linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
//       linkGPIO->writePin(LinkGPIO::Pin::SD, true);
// - 4) Read pins by using:
//       linkGPIO->setMode(LinkGPIO::Pin::SC, LinkGPIO::Direction::INPUT);
//       bool isHigh = linkGPIO->readPin(LinkGPIO::Pin::SC);
// - 5) Subscribe to SI falling:
//       linkGPIO->setSIInterrupts(true);
//       // (when SI changes from high to low, an IRQ will be generated)
// --------------------------------------------------------------------------
// considerations:
// - always set the SI terminal to an input!
// - call `reset()` when you finish doing GPIO stuff!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_GPIO_VERSION = "vLinkGPIO/v8.0.2";

/**
 * @brief A General Purpose Input-Output handler for the Link Port.
 */
class LinkGPIO {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu16 = Link::vu16;

  static constexpr int RCNT_GENERAL_PURPOSE = 1 << 15;
  static constexpr int SIOCNT_GENERAL_PURPOSE = 0;
  static constexpr int BIT_SI_INTERRUPT = 8;
  static constexpr u8 DATA_BITS[] = {2, 3, 1, 0};
  static constexpr u8 DIRECTION_BITS[] = {6, 7, 5, 4};

 public:
  enum class Pin { SI, SO, SD, SC };
  enum class Direction { INPUT, OUTPUT };

  /**
   * @brief Resets communication mode to General Purpose (same as
   * `Link::reset()`).
   * \warning Required to initialize the library!
   */
  void reset() {
    LINK_READ_TAG(LINK_GPIO_VERSION);

    Link::_REG_RCNT = RCNT_GENERAL_PURPOSE;
    Link::_REG_SIOCNT = SIOCNT_GENERAL_PURPOSE;
  }

  /**
   * @brief Configures a `pin` to use a `direction` (input or output).
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   * @param direction One of the enum values from `LinkGPIO::Direction`.
   */
  void setMode(Pin pin, Direction direction) {
    setBit(Link::_REG_RCNT, DIRECTION_BITS[(int)pin],
           direction == Direction::OUTPUT);
  }

  /**
   * @brief Returns the direction set at `pin`.
   */
  [[nodiscard]] Direction getMode(Pin pin) {
    return Direction(getBit(Link::_REG_RCNT, DIRECTION_BITS[(int)pin]));
  }

  /**
   * @brief Returns whether a `pin` is *HIGH* or not (when set as an input).
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   */
  [[nodiscard]] bool readPin(Pin pin) {
    return (Link::_REG_RCNT & (1 << DATA_BITS[(int)pin])) != 0;
  }

  /**
   * @brief Sets a `pin` to be high or not (when set as an output).
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   * @param isHigh `true` = HIGH, `false` = LOW.
   */
  void writePin(Pin pin, bool isHigh) {
    setBit(Link::_REG_RCNT, DATA_BITS[(int)pin], isHigh);
  }

  /**
   * @brief If it `isEnabled`, an IRQ will be generated when `SI` changes from
   * HIGH to LOW.
   * @param isEnabled Enable SI-falling interrupts.
   */
  void setSIInterrupts(bool isEnabled) {
    setBit(Link::_REG_RCNT, BIT_SI_INTERRUPT, isEnabled);
  }

  /**
   * @brief Returns whether SI-falling interrupts are enabled or not.
   */
  bool getSIInterrupts() { return getBit(Link::_REG_RCNT, BIT_SI_INTERRUPT); }

 private:
  int getBit(u16 reg, int bit) { return (reg >> bit) & 1; }

  void setBit(vu16& reg, int bit, bool data) {
    if (data)
      reg |= 1 << bit;
    else
      reg &= ~(1 << bit);
  }
};

extern LinkGPIO* linkGPIO;

#endif  // LINK_GPIO_H
