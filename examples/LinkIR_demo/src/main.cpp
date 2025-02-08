// (0) Include the header
#include "../../../lib/LinkIR.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkIR instance
LinkIR* linkIR = new LinkIR();

void init() {
  Common::initTTE();

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, LINK_IR_ISR_SERIAL);
  interrupt_add(INTR_TIMER3, []() {});

  // (3) Initialize the library
  // linkIR->activate();
}

int main() {
  init();

  bool isConnected = linkIR->activate();
  if (isConnected) {
    Common::log("Adapter connected!");
  } else {
    Common::log("NOT connected");
  }

  while (true) {
    VBlankIntrWait();
  }

  return 0;
}
