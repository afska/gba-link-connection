#include <tonc.h>

#include <string>

// (0) Include the header
#include "../lib/LinkConnection.h"

void log(std::string text);

// (1) Create a LinkConnection instance
LinkConnection* linkConnection = new LinkConnection();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  irq_init(NULL);

  // (2) Add the interrupt service routines
  irq_add(II_VBLANK, LINK_ISR_VBLANK);
  irq_add(II_SERIAL, LINK_ISR_SERIAL);
  irq_add(II_TIMER3, LINK_ISR_TIMER);
  irq_add(II_TIMER2, NULL);

  // (3) Initialize the library
  linkConnection->activate();
}

int main() {
  init();

  u16 data[LINK_MAX_PLAYERS];

  while (1) {
    // (4) Send/read messages messages
    u16 keys = ~REG_KEYS & KEY_ANY;
    u16 message = keys + 1;
    linkConnection->send(message);
    auto linkState = linkConnection->linkState.get();

    std::string output = "";
    if (linkState->isConnected()) {
      output += "Players: " + std::to_string(linkState->playerCount) + "\n";

      for (u32 i = 0; i < linkState->playerCount; i++) {
        while (linkState->hasMessage(i))
          data[i] = linkState->readMessage(i) - 1;

        output += "Player " + std::to_string(i) + ": " +
                  std::to_string(data[i]) + "\n";
      }

      output += "_sent: " + std::to_string(message) + "\n";
      output += "_self pID: " + std::to_string(linkState->currentPlayerId);
    } else
      output += std::string("Waiting...");
    log(output);

    VBlankIntrWait();
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}