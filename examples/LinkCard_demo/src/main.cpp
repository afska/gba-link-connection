// (0) Include the header
#include "../../../lib/LinkCard.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

// (1) Create a LinkCard instance
LinkCard* linkCard = new LinkCard();

void init() {
  Common::initTTE();

  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
}

int main() {
  init();

  // bool a = true;

  while (true) {
    std::string output = "LinkCard_demo (v8.0.0)\n\n";
    output += "Device: ";

    auto device = linkCard->getConnectedDevice();
    switch (device) {
      case LinkCard::ConnectedDevice::E_READER_LOADER_NEEDED: {
        output += "e-Reader";
        break;
      }
      case LinkCard::ConnectedDevice::DLC_LOADER: {
        output += "DLC Loader";
        break;
      }
      default: {
        output +=
            "??\n\n- Grab a GBA Link Cable\n- Use the 1P end here\n- Use "
            "the 2P end to e-Reader\n- Boot e-Reader in another GBA\n- Go to "
            "\"Communication\"\n- Choose \"To Game Boy Advance\"\n- Press A";
        break;
      }
    }

    // output += "Press A to send the loader";

    // if (Common::didPress(KEY_A, a)) {
    //   LinkRawCable lrc;
    //   lrc.activate();
    //   auto bb = lrc.transfer(0);
    //   Common::log(std::to_string(bb.data[1]));
    //   Common::waitForKey(KEY_DOWN);
    //   // linkCard->sendLoader()
    // }

    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
