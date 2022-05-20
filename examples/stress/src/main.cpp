#include <tonc.h>
#include <string>
#include "../../_lib/LinkConnection.h"
#include "../../_lib/interrupt.h"

// STRESS:
// This test sends consecutive values in a two-player setup.
// When a GBA receives something not equal to previousValue + 1, it hangs.
// It should work indefinitely (with no packet loss).

void log(std::string text);

LinkConnection* linkConnection = new LinkConnection();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  linkConnection->activate();
}

int main() {
  init();

  u16 localCounter = 0;
  u16 remoteCounter = 0;
  bool error = false;

  while (true) {
    std::string output = "";
    if (linkConnection->isConnected()) {
      auto playerCount = linkConnection->playerCount();
      auto currentPlayerId = linkConnection->currentPlayerId();
      auto remotePlayerId = !currentPlayerId;

      output += "Players: " + std::to_string(playerCount) + "\n";

      if (playerCount == 2) {
        linkConnection->send(localCounter + 1);
        localCounter++;
      }

      while (linkConnection->canRead(remotePlayerId)) {
        u16 message = linkConnection->read(remotePlayerId) - 1;
        if (message == remoteCounter) {
          remoteCounter++;
        } else {
          error = true;
          output += "ERROR!\nExpected " + std::to_string(remoteCounter) +
                    " but got " + std::to_string(message) + "\n";
        }
      }

      output += "(" + std::to_string(localCounter) + ", " +
                std::to_string(remoteCounter) + ")\n";
    } else {
      output += std::string("Waiting...");
      localCounter = 0;
      remoteCounter = 0;
      error = false;
    }

    log(output);

    if (error) {
      while (true)
        ;
    }

    linkConnection->consume();

    VBlankIntrWait();
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}