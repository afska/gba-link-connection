// (0) Include the header
#include "../../../lib/LinkPS2Keyboard.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

static std::string scanCodes = "";
static std::string output = "";
static u32 irqs = 0;
void SERIAL() {
  LINK_PS2_KEYBOARD_ISR_SERIAL();
  irqs++;
}

// (1) Create a LinkPS2Keyboard instance
LinkPS2Keyboard* linkPS2Keyboard = new LinkPS2Keyboard([](u8 scanCode) {
  // (4) Handle events in the callback sent to LinkPS2Keyboard's constructor!
  scanCodes += std::to_string(scanCode) + "|";
});

void init() {
  Common::initTTE();

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, SERIAL);
}

int main() {
  init();

  while (true) {
    std::string output = "LinkPS2Keyboard_demo (v8.0.0)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkPS2Keyboard->isActive()) {
      output +=
          "Press A to read keyboard input\n"
          "Press B to clear logs";

      if (keys & KEY_A) {
        // (3) Initialize the library
        Common::log("Waiting...");
        linkPS2Keyboard->activate();
        VBlankIntrWait();
        continue;
      }
    } else {
      if (keys & KEY_B) {
        scanCodes = "";
        irqs = 0;
      }
      output += std::to_string(irqs) + " - " + scanCodes;
    }

    // Print
    VBlankIntrWait();
    LINK_PS2_KEYBOARD_ISR_VBLANK();
    Common::log(output);
  }

  return 0;
}
