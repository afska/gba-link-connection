#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// (0) Include the header
#include "../../../lib/LinkUART.hpp"

void log(std::string text);
inline void VBLANK() {}

std::string buffer = "";

// (1) Create a LinkUART instance
LinkUART* linkUART = new LinkUART();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_UART_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
}

int main() {
  init();

  bool firstTransfer = false;

  while (true) {
    std::string output = "LinkUART_demo (v6.2.3)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkUART->isActive()) {
      firstTransfer = true;
      output += "START: Start listening...\n";
      output += "\n(stop: press L+R)\n";

      if ((keys & KEY_START) | (keys & KEY_SELECT)) {
        // (3) Initialize the library
        linkUART->activate();
        buffer = "";
      }
    } else {
      // Title
      output += "[uart]\n";
      if (firstTransfer) {
        log(output + "Waiting...");
        firstTransfer = false;
      }

      // (4) Send/read bytes
      if (linkUART->canRead()) {
        u8 newByte = linkUART->read();
        while (!linkUART->canSend())
          ;
        linkUART->send('z');
        buffer += (char)newByte;
        if (buffer.size() > 250)
          buffer = "";
      }
      output += buffer;

      // Cancel
      if ((keys & KEY_L) && (keys & KEY_R)) {
        linkUART->deactivate();
      }
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
