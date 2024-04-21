#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// (0) Include the header
#include "../../../lib/LinkPS2Mouse.hpp"

void log(std::string text);
inline void VBLANK() {}
inline void TIMER() {}

// (1) Create a LinkPS2Mouse instance
LinkPS2Mouse* linkPS2Mouse = new LinkPS2Mouse(2);

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_TIMER2, VBLANK);
  interrupt_enable(INTR_TIMER2);
}

int main() {
  init();

  while (true) {
    std::string output = "LinkPS2Mouse_demo (v6.3.0)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkPS2Mouse->isActive()) {
      output += "Press A to read mouse input";

      if (keys & KEY_A) {
        // (3) Initialize the library
        log("Waiting...");
        linkPS2Mouse->activate();
        VBlankIntrWait();
        continue;
      }
    } else {
      // (4) Get a report
      int data[3];
      linkPS2Mouse->report(data);
      log(std::to_string(data[0]) + ": " + "(" + std::to_string(data[1]) +
          ", " + std::to_string(data[2]) + ")");
    }

    // Print
    VBlankIntrWait();
    log(output);
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}
