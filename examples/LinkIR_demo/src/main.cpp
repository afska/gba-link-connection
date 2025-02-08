// (0) Include the header
#include "../../../lib/LinkIR.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkIR instance
LinkIR* linkIR = new LinkIR();

bool isConnected = false;

void init() {
  Common::initTTE();

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
  interrupt_add(INTR_SERIAL, LINK_IR_ISR_SERIAL);
  interrupt_add(INTR_TIMER3, []() {});

  // (3) Initialize the library
  isConnected = linkIR->activate();
}

int main() {
  init();

  linkIR->activate();

  bool a = true;

  while (true) {
    std::string output = "LinkIR_demo (v8.0.0)\n\n";

    output += std::string("IR adapter: ") +
              (isConnected ? "DETECTED" : "not detected");

    output += "\n\nA = Send NEC A=0x04, C=0x03";

    if (Common::didPress(KEY_A, a)) {
      Common::log("Sending...");
      linkIR->send<true>((u16[]){
          // leader
          9000, 4500,
          // address 0x04, LSB first (bits: 0,0,1,0,0,0,0,0)
          560, 560,   // bit0: 0
          560, 560,   // bit1: 0
          560, 1690,  // bit2: 1
          560, 560,   // bit3: 0
          560, 560,   // bit4: 0
          560, 560,   // bit5: 0
          560, 560,   // bit6: 0
          560, 560,   // bit7: 0
          // inverted Address 0xFB, LSB first (bits: 1,1,0,1,1,1,1,1)
          560, 1690,  // bit0: 1
          560, 1690,  // bit1: 1
          560, 560,   // bit2: 0
          560, 1690,  // bit3: 1
          560, 1690,  // bit4: 1
          560, 1690,  // bit5: 1
          560, 1690,  // bit6: 1
          560, 1690,  // bit7: 1
                      // command 0x03, LSB first (bits: 1,1,0,0,0,0,0,0)
          560, 1690,  // bit0: 1
          560, 1690,  // bit1: 1
          560, 560,   // bit2: 0
          560, 560,   // bit3: 0
          560, 560,   // bit4: 0
          560, 560,   // bit5: 0
          560, 560,   // bit6: 0
          560, 560,   // bit7: 0
          // inverted command 0xFC, LSB first (bits: 0,0,1,1,1,1,1,1)
          560, 560,   // bit0: 0
          560, 560,   // bit1: 0
          560, 1690,  // bit2: 1
          560, 1690,  // bit3: 1
          560, 1690,  // bit4: 1
          560, 1690,  // bit5: 1
          560, 1690,  // bit6: 1
          560, 1690,  // bit7: 1
                      // final burst
          560,
          // terminator
          0});
      Common::log("DONE!");
      Common::waitForKey(KEY_B);
    }

    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
