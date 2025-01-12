#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// (0) Include the header
#include "../../../lib/LinkPS2Keyboard.hpp"

void log(std::string text);
static std::string scanCodes = "";
static std::string output = "";
static u32 irqs = 0;
inline void VBLANK() {}
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
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, SERIAL);
  interrupt_enable(INTR_SERIAL);
}

int main() {
  init();

  while (true) {
    std::string output = "LinkPS2Keyboard_demo (v7.0.2)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkPS2Keyboard->isActive()) {
      output +=
          "Press A to read keyboard input\n"
          "Press B to clear logs";

      if (keys & KEY_A) {
        // (3) Initialize the library
        log("Waiting...");
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
    log(output);
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}
