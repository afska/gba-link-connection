#include <tonc.h>

#include <string>

// (0) Include the header
#include "lib/LinkConnection.h"

void log(std::string text);

// (1) Create a LinkConnection instance
LinkConnection* linkConnection = new LinkConnection();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  irq_init(NULL);
  irq_add(II_VBLANK, NULL);

  // (2) Add the interrupt service routine
  irq_add(II_SERIAL, ISR_serial);
}

int main() {
  init();

  while (1) {
    // (3) Run the `tick` method in your update loop
    u16 dataToBeSent = ~REG_KEYS & KEY_ANY;  // (keys state)
    auto linkState = linkConnection->tick(dataToBeSent);

    // (4) Process results
    tte_erase_screen();
    std::string output = "";
    if (linkState.isConnected()) {
      output += "Players: " + std::to_string(linkState.playerCount) + "\n";

      for (u32 i = 0; i < linkState.playerCount; i++)
        output += "Player " + std::to_string(i) + ": " +
                  std::to_string(linkState.data[i]) + "\n";

      output += "Self pID: " + std::to_string(linkState.currentPlayerId) + "\n";
    } else
      output += std::string("Waiting...") + "\n";
    log(output);

    VBlankIntrWait();
  }

  return 0;
}

void log(std::string text) {
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}