// (0) Include the header
#include "../../../lib/LinkGPIO.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

std::string mode(std::string name, LinkGPIO::Pin pin);
std::string value(std::string name, LinkGPIO::Pin pin);

// (1) Create a LinkGPIO instance
LinkGPIO* linkGPIO = new LinkGPIO();

static vu32 irqCount = 0;
void SERIAL() {
  irqCount++;
}

void init() {
  Common::initTTE();

  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, SERIAL);

  // (2) Initialize the library
  linkGPIO->reset();
}

int main() {
  init();

  bool left = true, up = true, right = true, down = true;
  bool start = true, select = true;

  while (true) {
    std::string output = "LinkGPIO_demo (v8.0.0)\n\n";

    // Commands
    u16 keys = ~REG_KEYS & KEY_ANY;

    // (3) Set modes
    if (Common::didPress(KEY_LEFT, left))
      linkGPIO->setMode(
          LinkGPIO::Pin::SI,
          (LinkGPIO::Direction)((int)linkGPIO->getMode(LinkGPIO::Pin::SI) + 1));
    if (Common::didPress(KEY_UP, up))
      linkGPIO->setMode(
          LinkGPIO::Pin::SD,
          (LinkGPIO::Direction)((int)linkGPIO->getMode(LinkGPIO::Pin::SD) + 1));
    if (Common::didPress(KEY_DOWN, down))
      linkGPIO->setMode(
          LinkGPIO::Pin::SC,
          (LinkGPIO::Direction)((int)linkGPIO->getMode(LinkGPIO::Pin::SC) + 1));
    if (Common::didPress(KEY_RIGHT, right))
      linkGPIO->setMode(
          LinkGPIO::Pin::SO,
          (LinkGPIO::Direction)((int)linkGPIO->getMode(LinkGPIO::Pin::SO) + 1));

    // (3) Write pins
    if (linkGPIO->getMode(LinkGPIO::Pin::SI) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SI, keys & KEY_L);
    if (linkGPIO->getMode(LinkGPIO::Pin::SO) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SO, keys & KEY_R);
    if (linkGPIO->getMode(LinkGPIO::Pin::SD) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SD, keys & KEY_B);
    if (linkGPIO->getMode(LinkGPIO::Pin::SC) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SC, keys & KEY_A);

    // (4) Subscribe to SI falling
    if (Common::didPress(KEY_START, start))
      linkGPIO->setSIInterrupts(!linkGPIO->getSIInterrupts());
    if (Common::didPress(KEY_SELECT, select))
      irqCount = 0;

    // Print modes
    output += mode("SI", LinkGPIO::Pin::SI);
    output += mode("SO", LinkGPIO::Pin::SO);
    output += mode("SD", LinkGPIO::Pin::SD);
    output += mode("SC", LinkGPIO::Pin::SC);

    // Print separator
    output += "\n---\n\n";

    // Print values
    output += value("SI", LinkGPIO::Pin::SI);
    output += value("SO", LinkGPIO::Pin::SO);
    output += value("SD", LinkGPIO::Pin::SD);
    output += value("SC", LinkGPIO::Pin::SC);

    // Print interrupts
    if (linkGPIO->getMode(LinkGPIO::Pin::SI) == LinkGPIO::Direction::INPUT)
      output +=
          "\nSI IRQ: " + std::to_string(linkGPIO->getSIInterrupts()) +
          (irqCount > 0
               ? " !!!" + (irqCount > 1 ? " (x" + std::to_string(irqCount) + ")"
                                        : "")
               : "");

    output +=
        "\n\n---\nUse the D-PAD to change modes\nUse the buttons to set "
        "values\nUse STA/SEL to toggle SI IRQ";

    // Print
    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}

std::string mode(std::string name, LinkGPIO::Pin pin) {
  return name + ": " +
         (linkGPIO->getMode(pin) == LinkGPIO::Direction::OUTPUT ? "OUTPUT\n"
                                                                : "INPUT\n");
}

std::string value(std::string name, LinkGPIO::Pin pin) {
  auto title = name + ": ";

  return (linkGPIO->getMode(pin) == LinkGPIO::Direction::INPUT ? "< " : "> ") +
         title + std::to_string(linkGPIO->readPin(pin)) + "\n";
}
