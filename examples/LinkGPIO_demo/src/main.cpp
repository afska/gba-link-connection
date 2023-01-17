#include <tonc.h>
#include <string>

#include "../../_lib/LinkGPIO.h"

void log(std::string text);

LinkGPIO* linkGPIO = new LinkGPIO();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  linkGPIO->reset();
}

int main() {
  init();

  while (true) {
    std::string output = "";

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
    output += "SI: INPUT\n";
    output +=
        std::string("SO: ") +
        (linkGPIO->getMode(LinkPin::SO) == LinkDirection::OUTPUT ? "OUTPUT\n"
                                                                 : "INPUT\n");
    output +=
        std::string("SD: ") +
        (linkGPIO->getMode(LinkPin::SD) == LinkDirection::OUTPUT ? "OUTPUT\n"
                                                                 : "INPUT\n");
    output +=
        std::string("SC: ") +
        (linkGPIO->getMode(LinkPin::SC) == LinkDirection::OUTPUT ? "OUTPUT\n"
                                                                 : "INPUT\n");

    // Separator
    output += "\n---\n\n";

    // Values
    output += "< SI: " + std::to_string(linkGPIO->readPin(LinkPin::SI)) + "\n";
    output +=
        linkGPIO->getMode(LinkPin::SO) == LinkDirection::INPUT
            ? "< SO: " + std::to_string(linkGPIO->readPin(LinkPin::SO)) + "\n"
            : "> SO: " + std::to_string(sendSOHigh) + "\n";
    output +=
        linkGPIO->getMode(LinkPin::SD) == LinkDirection::INPUT
            ? "< SD: " + std::to_string(linkGPIO->readPin(LinkPin::SD)) + "\n"
            : "> SD: " + std::to_string(sendSDHigh) + "\n";
    output +=
        linkGPIO->getMode(LinkPin::SC) == LinkDirection::INPUT
            ? "< SC: " + std::to_string(linkGPIO->readPin(LinkPin::SC)) + "\n"
            : "> SC: " + std::to_string(sendSCHigh) + "\n";

    // Set modes
    if (setSOOutput)
      linkGPIO->setMode(LinkPin::SO, LinkDirection::OUTPUT);
    if (setSDOutput)
      linkGPIO->setMode(LinkPin::SD, LinkDirection::OUTPUT);
    if (setSCOutput)
      linkGPIO->setMode(LinkPin::SC, LinkDirection::OUTPUT);
    if (setSOInput)
      linkGPIO->setMode(LinkPin::SO, LinkDirection::INPUT);
    if (setSDInput)
      linkGPIO->setMode(LinkPin::SD, LinkDirection::INPUT);
    if (setSCInput)
      linkGPIO->setMode(LinkPin::SC, LinkDirection::INPUT);

    // Set values
    if (linkGPIO->getMode(LinkPin::SO) == LinkDirection::OUTPUT)
      linkGPIO->writePin(LinkPin::SO, sendSOHigh);
    if (linkGPIO->getMode(LinkPin::SD) == LinkDirection::OUTPUT)
      linkGPIO->writePin(LinkPin::SD, sendSDHigh);
    if (linkGPIO->getMode(LinkPin::SC) == LinkDirection::OUTPUT)
      linkGPIO->writePin(LinkPin::SC, sendSCHigh);

    // Print
    log(output);

    while (REG_VCOUNT >= 160)
      ;  // wait till VDraw
    while (REG_VCOUNT < 160)
      ;  // wait till VBlank
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}