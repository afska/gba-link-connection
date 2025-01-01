// (0) Include the header
#include "../../../lib/LinkUART.hpp"

#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

void log(std::string text);
inline void VBLANK() {}

std::string received = "";
u32 lines = 0;

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
    std::string output = "LinkUART_demo (v7.1.0)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkUART->isActive()) {
      firstTransfer = true;
      output += "START: Start listening...\n";
      output += "\n(stop: press L+R)\n";

      if ((keys & KEY_START) | (keys & KEY_SELECT)) {
        // (3) Initialize the library
        linkUART->activate();
        received = "";
        lines = 0;
      }
    } else {
      // Title
      if (firstTransfer) {
        log(output + "Waiting...");
        firstTransfer = false;
      }

      // (4) Send/read bytes
      char buffer[256];
      if (linkUART->readLine(buffer, []() {
            u16 keys = ~REG_KEYS & KEY_ANY;
            return (keys & KEY_L) && (keys & KEY_R);
          })) {
        lines++;
        if (lines >= 18) {
          lines = 0;
          received = "";
        }

        received += std::string(buffer);

        linkUART->sendLine(">> gba", []() {
          u16 keys = ~REG_KEYS & KEY_ANY;
          return (keys & KEY_L) && (keys & KEY_R);
        });
      }

      output += received;

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
