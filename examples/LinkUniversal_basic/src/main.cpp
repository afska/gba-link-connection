// BASIC:
// This example sends the pressed buttons to other players.

// (0) Include the header
#include "../../../lib/LinkUniversal.hpp"

#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

void log(std::string text);
void waitFor(u16 key);

LinkUniversal* linkUniversal = nullptr;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  log("LinkUniversal_basic (v7.0.1)\n\n\n"
      "Press A to start\n\n\n"
      "hold LEFT on start:\n -> force cable\n\n"
      "hold RIGHT on start:\n -> force wireless\n\n"
      "hold UP on start:\n -> force wireless server\n\n"
      "hold DOWN on start:\n -> force wireless client\n\n"
      "hold B on start:\n -> set 2 players (wireless)");
  waitFor(KEY_A);
  u16 initialKeys = ~REG_KEYS & KEY_ANY;
  bool forceCable = initialKeys & KEY_LEFT;
  bool forceWireless = initialKeys & KEY_RIGHT;
  bool forceWirelessServer = initialKeys & KEY_UP;
  bool forceWirelessClient = initialKeys & KEY_DOWN;
  LinkUniversal::Protocol protocol =
      forceCable            ? LinkUniversal::Protocol::CABLE
      : forceWireless       ? LinkUniversal::Protocol::WIRELESS_AUTO
      : forceWirelessServer ? LinkUniversal::Protocol::WIRELESS_SERVER
      : forceWirelessClient ? LinkUniversal::Protocol::WIRELESS_CLIENT
                            : LinkUniversal::Protocol::AUTODETECT;
  u32 maxPlayers = (initialKeys & KEY_B) ? 2 : LINK_UNIVERSAL_MAX_PLAYERS;

  // (1) Create a LinkUniversal instance
  linkUniversal =
      new LinkUniversal(protocol, "LinkUNI",
                        (LinkUniversal::CableOptions){
                            .baudRate = LinkCable::BAUD_RATE_1,
                            .timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                            .interval = LINK_CABLE_DEFAULT_INTERVAL,
                            .sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID},
                        (LinkUniversal::WirelessOptions){
                            .retransmission = true,
                            .maxPlayers = maxPlayers,
                            .timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                            .interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                            .sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID},
                        __qran_seed);

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // (3) Initialize the library
  linkUniversal->activate();

  u16 data[LINK_UNIVERSAL_MAX_PLAYERS];
  for (u32 i = 0; i < LINK_UNIVERSAL_MAX_PLAYERS; i++)
    data[i] = 0;

  while (true) {
    // (4) Sync
    linkUniversal->sync();

    // (5) Send/read messages
    u16 keys = ~REG_KEYS & KEY_ANY;
    linkUniversal->send(keys + 1);  // (avoid using 0)

    std::string output = "";
    if (linkUniversal->isConnected()) {
      u8 playerCount = linkUniversal->playerCount();
      u8 currentPlayerId = linkUniversal->currentPlayerId();

      output += "Players: " + std::to_string(playerCount) + "\n";

      output += "(";
      for (u32 i = 0; i < playerCount; i++) {
        while (linkUniversal->canRead(i))
          data[i] = linkUniversal->read(i) - 1;  // (avoid using 0)

        output += std::to_string(data[i]) + (i + 1 == playerCount ? "" : ", ");
      }
      output += ")\n";
      output += "_keys: " + std::to_string(keys) + "\n";
      output += "_pID: " + std::to_string(currentPlayerId);
    } else {
      output +=
          "Waiting... [" + std::to_string(linkUniversal->getState()) + "]";
      output += "<" + std::to_string(linkUniversal->getMode()) + ">";
      if (linkUniversal->getMode() == LinkUniversal::Mode::LINK_WIRELESS)
        output += "          (" +
                  std::to_string(linkUniversal->getWirelessState()) + ")";
      output += "\n_wait: " + std::to_string(linkUniversal->_getWaitCount());
      output += "\n_subW: " + std::to_string(linkUniversal->_getSubWaitCount());

      for (u32 i = 0; i < LINK_UNIVERSAL_MAX_PLAYERS; i++)
        data[i] = 0;
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

void waitFor(u16 key) {
  u16 keys;
  do {
    keys = ~REG_KEYS & KEY_ANY;
  } while (!(keys & key));
}
