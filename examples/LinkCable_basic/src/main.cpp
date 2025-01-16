// BASIC:
// This example sends the pressed buttons to other players.

// (0) Include the header
#include "../../../lib/LinkCable.hpp"

#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

void log(std::string text);

// (1) Create a LinkCable instance
LinkCable* linkCable = new LinkCable();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // (3) Initialize the library
  linkCable->activate();
}

int main() {
  init();

  u16 data[LINK_CABLE_MAX_PLAYERS];
  for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
    data[i] = 0;

  while (true) {
    // (4) Sync
    linkCable->sync();

    // (5) Send/read messages
    u16 keys = ~REG_KEYS & KEY_ANY;
    linkCable->send(keys + 1);  // (avoid using 0)

    std::string output = "LinkCable_basic (v7.0.3)\n\n";
    if (linkCable->isConnected()) {
      u8 playerCount = linkCable->playerCount();
      u8 currentPlayerId = linkCable->currentPlayerId();

      output += "Players: " + std::to_string(playerCount) + "\n";

      output += "(";
      for (u32 i = 0; i < playerCount; i++) {
        while (linkCable->canRead(i))
          data[i] = linkCable->read(i) - 1;  // (avoid using 0)

        output += std::to_string(data[i]) + (i + 1 == playerCount ? "" : ", ");
      }
      output += ")\n";
      output += "_keys: " + std::to_string(keys) + "\n";
      output += "_pID: " + std::to_string(currentPlayerId);
    } else {
      output += std::string("Waiting...");
    }

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
