// BASIC:
// This example sends the pressed buttons to other players.

// (0) Include the header
#include "../../../lib/LinkCable.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkCable instance
LinkCable* linkCable = new LinkCable();

void init() {
  Common::initTTE();

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_add(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_add(INTR_TIMER3, LINK_CABLE_ISR_TIMER);

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

    std::string output = "LinkCable_basic (v8.0.0)\n\n";
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
    Common::log(output);
  }

  return 0;
}
