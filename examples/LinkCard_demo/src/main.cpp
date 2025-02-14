// (0) Include the header
// #include "../../../lib/LinkCard.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkCard instance
// LinkCard* linkCard = new LinkCard();

void init() {
  Common::initTTE();

  // (2) Add the required interrupt service routines
  // interrupt_init();
  // interrupt_add(INTR_VBLANK, []() {});
  // interrupt_add(INTR_SERIAL, LINK_CARD_ISR_SERIAL);
  // interrupt_add(INTR_TIMER2, []() {});
  // interrupt_add(INTR_TIMER3, []() {});

  // (3) Initialize the library
  // linkCard->activate();
}

int main() {
  init();

  while (true) {
    std::string output = "LinkCard_demo (v8.0.0)\n\n";

    output += "// TODO: IMPLEMENT";

    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
