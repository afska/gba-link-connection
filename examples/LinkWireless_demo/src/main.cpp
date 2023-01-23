#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

void log(std::string text);

// (1) Create a LinkWireless instance
LinkWireless* linkWireless = new LinkWireless();

void init() {
  linkWireless->debug = [](std::string text) { log(text); };  // TODO: REMOVE

  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Initialize the library
  // linkWireless->activate(); // TODO: RECOVER
}

int main() {
  init();

  bool activating = false;
  bool hosting = false;
  bool connecting = false;

  while (true) {
    // std::string output = "";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if ((keys & KEY_START) && !activating) {
      log("Trying...");
      activating = true;
      if (linkWireless->activate())
        log("Activated! :)");
      else
        log("Activation failed! :(");
    }

    if (activating && !(keys & KEY_START))
      activating = false;

    if ((keys & KEY_L) && !hosting) {
      log("Hosting...");
      hosting = true;
      if (linkWireless->host(std::vector<u32>{0x0c020002, 0x00005ce1,
                                              0x00000000, 0x09000040,
                                              0xc1cfc8cd, 0x00ffccbb})) {
        log("Hosting ok. Listening...");
        u16 newId = 0;
        u32 tryId = 0;
        do {
          newId = linkWireless->getNewConnectionId();
          tryId++;
          log("Hosting ok. " + std::to_string(newId) + " Listening... " +
              std::to_string(tryId));
        } while (newId == 0 || newId == 1);
        log("CONNECTED!" + std::to_string(newId));
        while (true)
          ;
      } else {
        log("Hosting error");
        while (true)
          ;
      }
    }

    if (hosting && !(keys & KEY_L))
      hosting = false;

    if (!connecting && (keys & KEY_R)) {
      log("Searching...");
      connecting = true;
      std::vector<u32> data;
      if (linkWireless->getBroadcasts(data)) {
        std::string data = "Search!\n";
        for (auto& dat : data)
          data += std::to_string(dat) + "\n";
        log(data);
      } else
        log("Search failed :(");
    }

    if (connecting && !(keys & KEY_R))
      connecting = false;

    // if ((keys & KEY_A) && !sending) {
    //   log("Sending...");
    //   sending = true;
    //   if (linkWireless->broadcast(std::vector<u32>{80, 105, 111, 108, 97})) {
    //     log("Sent! :)");
    //     if (linkWireless->startHost())
    //       log("Host started!");
    //     else
    //       log("Host failed -.-");
    //   } else
    //     log("Broadcast failed :(");
    // }

    // if (sending && !(keys & KEY_A))
    //   sending = false;

    // if ((keys & KEY_B) && !reading) {
    //   log("Reading...");
    //   reading = true;
    //   std::vector<u32> data;
    //   if (linkWireless->read(data)) {
    //     std::string data = "Read!\n";
    //     for (auto& dat : data)
    //       data += std::to_string(dat) + "\n";
    //     log(data);
    //   } else
    //     log("Read failed :(");
    // }

    // if (reading && !(keys & KEY_B))
    //   reading = false;

    // output += "Testing...";

    // // Print
    // log(output);

    while (REG_VCOUNT >= 160)
      ;  // wait till VDraw
    while (REG_VCOUNT < 160)
      ;  // wait till VBlank
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}