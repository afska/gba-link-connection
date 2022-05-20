#include <tonc.h>
#include <string>
#include "interrupt.h"

#include "../../_lib/LinkConnection.h"

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

  u16 counter = 0;
  u16 remoteCounter = 0;
  bool error = false;

  while (true) {
    auto linkState = linkConnection->linkState.get();

    std::string output = "";
    if (linkState->isConnected()) {
      output += "Players: " + std::to_string(linkState->playerCount) + "\n";

      if (linkState->playerCount == 2) {
        linkConnection->send(counter + 1);
        counter++;
      }

      while (linkState->hasMessage(!linkState->currentPlayerId)) {
        u16 msg = linkState->readMessage(!linkState->currentPlayerId) - 1;
        if (msg == remoteCounter) {
          remoteCounter++;
        } else {
          error = true;
          output += "ERROR!\nExpected " + std::to_string(remoteCounter) +
                    " but got " + std::to_string(msg) + "\n";
        }
      }

      output += "(" + std::to_string(counter) + ", " +
                std::to_string(remoteCounter) + ")\n";
    } else {
      output += std::string("Waiting...");
    }

    log(output);

    if (error) {
      while (true)
        ;
    }

    VBlankIntrWait();
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}