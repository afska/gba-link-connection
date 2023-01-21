#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

void log(std::string text);

LinkWireless* linkWireless = NULL;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Initialize the library
  linkWireless->activate();
}

int main() {
  init();

  while (true) {
    std::string output = "";
    // u16 keys = ~REG_KEYS & KEY_ANY;

    output += "Testing...";

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