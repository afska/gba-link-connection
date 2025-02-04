// (0) Include the header
#include "../../../lib/LinkCube.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

bool a = true, b = true, l = true;

// (1) Create a LinkCube instance
LinkCube* linkCube = new LinkCube();

void init() {
  Common::initTTE();

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
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
        "LinkCube_demo (v8.0.0)" + std::string(reset ? " !RESET!" : "") +
        "\n\nPress A to send\nPress B to clear\n (L = "
        "+1024)\n\nLast sent: " +
        std::to_string(counter) +
        "\n(pending = " + std::to_string(linkCube->pendingCount()) +
        ")\n\nReceived:\n" + received;

    // (4) Send 32-bit values
    if (Common::didPress(KEY_A, a)) {
      counter++;
      linkCube->send(counter);
    }

    // +1024
    if (Common::didPress(KEY_L, l)) {
      counter += 1024;
      linkCube->send(counter);
    }

    // (5) Read 32-bit values
    while (linkCube->canRead()) {
      received += std::to_string(linkCube->read()) + ", ";
    }

    // Clear
    if (Common::didPress(KEY_B, b))
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
    Common::log(output);
  }

  return 0;
}
