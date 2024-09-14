// (0) Include the header
#include "../../../lib/LinkCube.hpp"

#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

void log(std::string text);
void wait(u32 verticalLines);
bool didPress(unsigned short key, bool& pressed);
inline void VBLANK() {}

bool a = false, b = false, l = false;

// (1) Create a LinkCube instance
LinkCube* linkCube = new LinkCube();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
}

int main() {
  init();

  // (3) Initialize the library
  linkCube->activate();

  u32 counter = 0;
  std::string received = "";
  bool reset = false;
  u32 vblanks = 0;

  while (true) {
    // Title
    std::string output =
        "LinkCube_demo (v7.0.1)" + std::string(reset ? " !RESET!" : "") +
        "\n\nPress A to send\nPress B to clear\n (L = "
        "+1024)\n\nLast sent: " +
        std::to_string(counter) +
        "\n(pending = " + std::to_string(linkCube->pendingCount()) +
        ")\n\nReceived:\n" + received;

    // (4) Send 32-bit values
    if (didPress(KEY_A, a)) {
      counter++;
      linkCube->send(counter);
    }

    // +1024
    if (didPress(KEY_L, l)) {
      counter += 1024;
      linkCube->send(counter);
    }

    // (5) Read 32-bit values
    while (linkCube->canRead()) {
      received += std::to_string(linkCube->read()) + ", ";
    }

    // Clear
    if (didPress(KEY_B, b))
      received = "";

    // Reset warning
    if (linkCube->didReset()) {
      counter = 0;
      reset = true;
      vblanks = 0;
    }
    if (reset) {
      vblanks++;
      if (vblanks > 60)
        reset = false;
    }

    // Print
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

void wait(u32 verticalLines) {
  u32 count = 0;
  u32 vCount = REG_VCOUNT;

  while (count < verticalLines) {
    if (REG_VCOUNT != vCount) {
      count++;
      vCount = REG_VCOUNT;
    }
  };
}

bool didPress(u16 key, bool& pressed) {
  u16 keys = ~REG_KEYS & KEY_ANY;
  bool isPressedNow = false;
  if ((keys & key) && !pressed) {
    pressed = true;
    isPressedNow = true;
  }
  if (pressed && !(keys & key))
    pressed = false;
  return isPressedNow;
}
