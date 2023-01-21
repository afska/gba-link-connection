#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkSPI.h"

void log(std::string text);

// (1) Create a LinkSPI instance
LinkSPI* linkSPI = new LinkSPI();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  bool firstTransfer = false;

  while (true) {
    std::string output = "";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkSPI->isActive()) {
      firstTransfer = true;
      output += "START: Set as Master\n";
      output += "SELECT: Set as Slave\n";
      output +=
          "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n[!] to test this demo...\n      "
          "...use a GBC Link Cable!";

      if ((keys & KEY_START) | (keys & KEY_SELECT)) {
        // (2) Initialize the library
        linkSPI->activate((keys & KEY_START) ? LinkSPI::Mode::MASTER_256KBPS
                                             : LinkSPI::Mode::SLAVE,
                          true);  // see `waitMode` in README.md
      }
    } else {
      // Title
      auto modeName =
          linkSPI->getMode() == LinkSPI::Mode::SLAVE ? "Slave" : "Master";
      output += std::string("[") + modeName + "]\n";
      output += "(stop: L+R)\n\n";
      if (firstTransfer)
        log(output + "Waiting...");

      // (3) Exchange 32-bit data with the other end
      u32 remoteKeys = linkSPI->transfer(keys, []() {
        u16 keys = ~REG_KEYS & KEY_ANY;
        return (keys & KEY_L) && (keys & KEY_R);
      });
      output += "local:  " + std::to_string(keys) + "\n";
      output += "remote: " + std::to_string(remoteKeys) + "\n";
      firstTransfer = false;

      // Cancel
      if ((keys & KEY_L) && (keys & KEY_R))
        linkSPI->deactivate();
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