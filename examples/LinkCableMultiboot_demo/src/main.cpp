#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

#include "../../../lib/LinkCable.hpp"
// (0) Include the header
#include "../../../lib/LinkCableMultiboot.hpp"

void log(std::string text);

LinkCable* linkCable = new LinkCable();
// (1) Create a LinkCableMultiboot instance
LinkCableMultiboot* linkCableMultiboot = new LinkCableMultiboot();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
}

int main() {
  init();

  // Hardcoded ROM length
  // This is optional, you could also use `LINK_CABLE_MULTIBOOT_MAX_ROM_SIZE`
  // (but the transfer will be painfully slow)
  u32 romSize = 39264;
  // Note that this project's Makefile pads the ROM to a 0x10 boundary
  // (as required for Multiboot).

  // Each player's input
  u16 data[LINK_CABLE_MAX_PLAYERS];
  for (u32 i = 0; i < LINK_CABLE_MAX_PLAYERS; i++)
    data[i] = 0;

  bool isSenderMode = true;
  LinkCableMultiboot::Result result = LinkCableMultiboot::Result::CANCELED;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // Sender options
    if (isSenderMode) {
      if (result != LinkCableMultiboot::Result::SUCCESS)
        log("LinkCableMultiboot_demo\n  (v6.3.0)\n\nPress START to send the "
            "ROM...\nPress B to set client mode...");

      if (keys & KEY_START) {
        log("Sending... (SELECT to cancel)");
        linkCable->deactivate();

        // (3) Send the ROM
        result =
            linkCableMultiboot->sendRom((const u8*)MEM_EWRAM, romSize, []() {
              u16 keys = ~REG_KEYS & KEY_ANY;
              return keys & KEY_SELECT;
            });

        // Print results and wait
        log("Result: " + std::to_string(result) + "\n" +
            "Press A to continue...");
        do {
          keys = ~REG_KEYS & KEY_ANY;
        } while (!(keys & KEY_A));

        // Switch to client mode if it worked
        if (result == LinkCableMultiboot::Result::SUCCESS) {
          linkCable->activate();
          VBlankIntrWait();
          continue;
        }
      }

      // Switch to client mode manually (for slaves)
      if (keys & KEY_B) {
        isSenderMode = false;
        linkCable->activate();
        VBlankIntrWait();
        continue;
      }

      // In sender mode, don't continue until the ROM is sent successfully
      if (result != LinkCableMultiboot::Result::SUCCESS) {
        VBlankIntrWait();
        continue;
      }
    }

    // ---
    // Client mode
    // ---

    linkCable->sync();
    linkCable->send(keys + 1);

    std::string output = "";
    if (linkCable->isConnected()) {
      u8 playerCount = linkCable->playerCount();
      u8 currentPlayerId = linkCable->currentPlayerId();

      output += "Players: " + std::to_string(playerCount) + "\n";

      output += "(";
      for (u32 i = 0; i < playerCount; i++) {
        while (linkCable->canRead(i))
          data[i] = linkCable->read(i) - 1;

        output += std::to_string(data[i]) + (i + 1 == playerCount ? "" : ", ");
      }
      output += ")\n";
      output += "_keys: " + std::to_string(keys) + "\n";
      output += "_pID: " + std::to_string(currentPlayerId);
    } else {
      output += std::string("Waiting... ");
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