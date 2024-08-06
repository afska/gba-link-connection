#ifndef LINK_GPIO_H
#define LINK_GPIO_H

// --------------------------------------------------------------------------
// A GPIO handler for the Link Port.
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
// - call reset() when you finish doing GPIO stuff!
// --------------------------------------------------------------------------

#include "_link_common.h"

class LinkGPIO {
 private:
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int RCNT_GENERAL_PURPOSE = (1 << 15);
  static constexpr int SIOCNT_GENERAL_PURPOSE = 0;
  static constexpr int BIT_SI_INTERRUPT = 8;
  static inline int _GET_BIT(volatile u16 REG, int BIT) {
    return (REG >> BIT) & 1;
  }
  static inline void _SET_BIT(volatile u16& REG, int BIT, bool DATA) {
    if (DATA)
      REG |= 1 << BIT;
    else
      REG &= ~(1 << BIT);
  }
  static constexpr u8 DATA_BITS[] = {2, 3, 1, 0};
  static constexpr u8 DIRECTION_BITS[] = {6, 7, 5, 4};

 public:
  enum Pin { SI, SO, SD, SC };
  enum Direction { INPUT, OUTPUT };

  /**
   * @brief Resets communication mode to General Purpose.
   * \warning Required to initialize the library!
   */
  void reset() {
    Link::_REG_RCNT = RCNT_GENERAL_PURPOSE;
    Link::_REG_SIOCNT = SIOCNT_GENERAL_PURPOSE;
  }

  /**
   * @brief Configures a `pin` to use a `direction` (input or output).
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   * @param direction One of the enum values from `LinkGPIO::Direction`.
   */
  void setMode(Pin pin, Direction direction) {
    _SET_BIT(Link::_REG_RCNT, DIRECTION_BITS[pin],
             direction == Direction::OUTPUT);
  }

  /**
   * @brief Returns the direction set at `pin`.
   */
  [[nodiscard]] Direction getMode(Pin pin) {
    return Direction(_GET_BIT(Link::_REG_RCNT, DIRECTION_BITS[pin]));
  }

  /**
   * @brief Returns whether a `pin` is *HIGH* or not (when set as an input).
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   */
  [[nodiscard]] bool readPin(Pin pin) {
    return (Link::_REG_RCNT & (1 << DATA_BITS[pin])) != 0;
  }

  /**
   * @brief Sets a `pin` to be high or not (when set as an output).
   *
   * @param pin One of the enum values from `LinkGPIO::Pin`.
   * @param isHigh `true` = HIGH, `false` = LOW.
   */
  void writePin(Pin pin, bool isHigh) {
    _SET_BIT(Link::_REG_RCNT, DATA_BITS[pin], isHigh);
  }

  /**
   * @brief If it `isEnabled`, an IRQ will be generated when `SI` changes from
   * HIGH to LOW.
   *
   * @param isEnabled Enable SI-falling interrupts.
   */
  void setSIInterrupts(bool isEnabled) {
    _SET_BIT(Link::_REG_RCNT, BIT_SI_INTERRUPT, isEnabled);
  }
};

extern LinkGPIO* linkGPIO;

#endif  // LINK_GPIO_H
