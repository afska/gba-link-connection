// (0) Include the header
#include "../../../lib/LinkPS2Mouse.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkPS2Mouse instance
LinkPS2Mouse* linkPS2Mouse = new LinkPS2Mouse(2);

void init() {
  Common::initTTE();

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_TIMER2, []() {});

  // B = SoftReset
  REG_KEYCNT = 0b10 | (1 << 14);
  interrupt_add(INTR_KEYPAD, Common::ISR_reset);
}

int main() {
  init();

  while (true) {
    std::string output = "LinkPS2Mouse_demo (v8.0.3)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkPS2Mouse->isActive()) {
      output +=
          "Press A to read mouse input\n"
          "Press B to cancel";

      if (keys & KEY_A) {
        // (3) Initialize the library
        Common::log("Waiting...");
        linkPS2Mouse->activate();
        VBlankIntrWait();
        continue;
      }
    } else {
      // (4) Get a report
      int data[3];
      linkPS2Mouse->report(data);
      output += std::to_string(data[0]) + ": " + "(" + std::to_string(data[1]) +
                ", " + std::to_string(data[2]) + ")";
    }

    // Print
    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
