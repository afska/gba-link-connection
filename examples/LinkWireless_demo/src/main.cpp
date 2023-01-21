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

  while (true) {
    // std::string output = "";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if ((keys & KEY_START) && !activating) {
      log("Trying...");
      activating = true;
      linkWireless->activate();
    }

    if (activating && !(keys & KEY_START)) {
      activating = false;
    }

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