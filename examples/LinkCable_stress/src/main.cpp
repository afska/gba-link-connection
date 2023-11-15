#include "main.h"
#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// STRESS:
// This example sends consecutive values in a two-player setup.
// When a GBA receives something not equal to previousValue + 1, it hangs.
// It should work indefinitely (with no packet loss).

void log(std::string text);

#ifndef USE_LINK_UNIVERSAL
LinkCable* linkCable = new LinkCable();
LinkCable* link = linkCable;
#endif
#ifdef USE_LINK_UNIVERSAL
LinkUniversal* linkUniversal = new LinkUniversal();
LinkUniversal* link = linkUniversal;
#endif

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  interrupt_init();

#ifndef USE_LINK_UNIVERSAL
  // LinkCable
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#endif
#ifdef USE_LINK_UNIVERSAL
  // LinkUniversal
  interrupt_set_handler(INTR_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#endif

  link->activate();
}

int main() {
  init();

  u16 localCounter = 0;
  u16 remoteCounter = 0;
  bool error = false;

  while (true) {
    link->sync();

#ifndef USE_LINK_UNIVERSAL
    std::string output = "LinkCable\n\n";
#endif
#ifdef USE_LINK_UNIVERSAL
    std::string output = "LinkUniversal\n\n";
#endif

    if (link->isConnected()) {
      auto playerCount = link->playerCount();
      auto currentPlayerId = link->currentPlayerId();
      auto remotePlayerId = !currentPlayerId;

      output += "Players: " + std::to_string(playerCount) + "\n";

      if (playerCount == 2) {
        link->send(localCounter + 1);
        localCounter++;
      }

      while (link->canRead(remotePlayerId)) {
        u16 message = link->read(remotePlayerId) - 1;
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
      output += "Waiting...";
      localCounter = 0;
      remoteCounter = 0;
      error = false;
    }

    VBlankIntrWait();
    log(output);

    if (error) {
      while (true)
        ;
    }
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}