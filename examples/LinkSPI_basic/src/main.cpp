#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkSPI.h"

#define PLAYERS 2

void log(std::string text);

LinkSPI* linkSPI = NULL;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  while (true) {
    std::string output = "";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (linkSPI == NULL) {
      output += "START: Set as Master\n";
      output += "SELECT: Set as Slave";

      // (1) Create a LinkSPI instance
      if (keys & KEY_START)
        linkSPI = new LinkSPI(LinkSPI::Mode::MASTER_256KBPS);
      if (keys & KEY_SELECT)
        linkSPI = new LinkSPI(LinkSPI::Mode::SLAVE);
    } else {
      // Title
      auto modeName =
          linkSPI->getMode() == LinkSPI::Mode::SLAVE ? "Slave" : "Master";
      output += std::string("[") + modeName + "]\n";
      output += "(stop: L+R)\n\n";
      if (!linkSPI->isActive())
        log(output + "Waiting...");

      // Transfer
      u32 remoteKeys = linkSPI->transfer(keys, []() {
        u16 keys = ~REG_KEYS & KEY_ANY;
        return (keys & KEY_L) && (keys & KEY_R);
      });
      output += "local:  " + std::to_string(keys) + "\n";
      output += "remote: " + std::to_string(remoteKeys) + "\n";

      // Cancel
      if ((keys & KEY_L) && (keys & KEY_R)) {
        linkSPI->deactivate();
        linkSPI = NULL;
      }
    }

    // Print
    log(output);

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