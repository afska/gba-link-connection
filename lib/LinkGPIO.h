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
//       linkGPIO->setMode(LinkPin::SD, LinkDirection::OUTPUT);
//       linkGPIO->writePin(LinkPIN::SD, true);
// - 4) Read pins by using:
//       linkGPIO->setMode(LinkPin::SC, LinkDirection::INPUT);
//       bool isHigh = linkGPIO->readPin(LinkPin::SC);
// - 5) Subscribe to SI falling:
//       linkGPIO->setSIInterrupts(true);
//       // (when SI changes from high to low, an IRQ will be generated)
// --------------------------------------------------------------------------
// `setMode` restrictions:
// always set the SI terminal to an input!
// --------------------------------------------------------------------------

#include <tonc_core.h>

#define LINK_GPIO_MODE 15
#define LINK_GPIO_SET(REG, BIT, DATA) \
  if (DATA)                           \
    LINK_GPIO_SET_HIGH(REG, BIT);     \
  else                                \
    LINK_GPIO_SET_LOW(REG, BIT);
#define LINK_GPIO_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_GPIO_SET_LOW(REG, BIT) REG &= ~(1 << BIT)
#define LINK_GPIO_SI_INTERRUPT_BIT 8

enum LinkPin { SI, SO, SD, SC };
enum LinkDirection { INPUT, OUTPUT };
const u8 LINK_PIN_DATA_BITS[] = {2, 3, 1, 0};
const u8 LINK_PIN_DIRECTION_BITS[] = {6, 7, 5, 4};

class LinkGPIO {
 public:
  void reset() {
    REG_RCNT = 1 << LINK_GPIO_MODE;
    REG_SIOCNT = 0;
  }

  void setMode(LinkPin pin, LinkDirection direction) {
    if (pin == LinkPin::SI && LinkDirection::OUTPUT)
      return;  // (disabled for safety reasons)

    LINK_GPIO_SET(REG_RCNT, LINK_PIN_DIRECTION_BITS[pin],
                  direction == LinkDirection::OUTPUT);
  }

  bool readPin(LinkPin pin) {
    return (REG_RCNT & (1 << LINK_PIN_DATA_BITS[pin])) != 0;
  }

  void writePin(LinkPin pin, bool data) {
    LINK_GPIO_SET(REG_RCNT, LINK_PIN_DATA_BITS[pin], data);
  }

  void setSIInterrupts(bool isEnabled) {
    LINK_GPIO_SET(REG_RCNT, LINK_GPIO_SI_INTERRUPT_BIT, isEnabled);
  }
};

extern LinkGPIO* linkGPIO;

#endif  // LINK_GPIO_H
