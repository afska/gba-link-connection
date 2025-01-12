// (0) Include the header
#include "../../../lib/LinkGPIO.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

std::string mode(std::string name, LinkGPIO::Pin pin);
std::string value(std::string name, LinkGPIO::Pin pin, bool isHigh);

// (1) Create a LinkGPIO instance
LinkGPIO* linkGPIO = new LinkGPIO();

void init() {
  Common::initTTE();

  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});

  // (2) Initialize the library
  linkGPIO->reset();
}

int main() {
  init();

  while (true) {
    // (3) Use the pins
    std::string output = "LinkGPIO_demo (v7.1.0)\n\n";

    // Commands
    u16 keys = ~REG_KEYS & KEY_ANY;
    bool setSOOutput = keys & KEY_L;
    bool setSDOutput = keys & KEY_UP;
    bool setSCOutput = keys & KEY_R;
    bool setSOInput = keys & KEY_LEFT;
    bool setSDInput = keys & KEY_DOWN;
    bool setSCInput = keys & KEY_RIGHT;
    bool sendSOHigh = keys & KEY_B;
    bool sendSDHigh = keys & KEY_START;
    bool sendSCHigh = keys & KEY_A;

    // Modes
    output += mode("SI", LinkGPIO::Pin::SI);
    output += mode("SO", LinkGPIO::Pin::SO);
    output += mode("SD", LinkGPIO::Pin::SD);
    output += mode("SC", LinkGPIO::Pin::SC);

    // Separator
    output += "\n---\n\n";

    // Values
    output += value("SI", LinkGPIO::Pin::SI, false);
    output += value("SO", LinkGPIO::Pin::SO, sendSOHigh);
    output += value("SD", LinkGPIO::Pin::SD, sendSDHigh);
    output += value("SC", LinkGPIO::Pin::SC, sendSCHigh);

    // Set modes
    if (setSOOutput)
      linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    if (setSDOutput)
      linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    if (setSCOutput)
      linkGPIO->setMode(LinkGPIO::Pin::SC, LinkGPIO::Direction::OUTPUT);
    if (setSOInput)
      linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::INPUT);
    if (setSDInput)
      linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::INPUT);
    if (setSCInput)
      linkGPIO->setMode(LinkGPIO::Pin::SC, LinkGPIO::Direction::INPUT);

    // Set values
    if (linkGPIO->getMode(LinkGPIO::Pin::SO) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SO, sendSOHigh);
    if (linkGPIO->getMode(LinkGPIO::Pin::SD) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SD, sendSDHigh);
    if (linkGPIO->getMode(LinkGPIO::Pin::SC) == LinkGPIO::Direction::OUTPUT)
      linkGPIO->writePin(LinkGPIO::Pin::SC, sendSCHigh);

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

std::string value(std::string name, LinkGPIO::Pin pin, bool isHigh) {
  auto title = name + ": ";

  return linkGPIO->getMode(pin) == LinkGPIO::Direction::INPUT
             ? "< " + title + std::to_string(linkGPIO->readPin(pin)) + "\n"
             : "> " + title + std::to_string(isHigh) + "\n";
}
