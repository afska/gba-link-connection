// (0) Include the header
#include "../../../lib/LinkCard.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

extern "C" {
#include "../../_lib/libgbfs/gbfs.h"
}

static const GBFS_FILE* fs = find_first_gbfs_file(0);

// (1) Create a LinkCard instance
LinkCard* linkCard = new LinkCard();

void init() {
  Common::initTTE();

  interrupt_init();
  interrupt_add(INTR_VBLANK, []() {});
}

int main() {
  init();

  // Ensure there are GBFS files
  if (fs == NULL) {
    Common::log("! GBFS file not found");
    while (true)
      ;
  } else if (gbfs_get_nth_obj(fs, 0, NULL, NULL) == NULL) {
    Common::log("! No files found (GBFS)");
    while (true)
      ;
  }

  bool a = true;

  while (true) {
    std::string output = "LinkCard_demo (v8.0.0)\n\n";
    output += "Device: ";

    auto device = linkCard->getConnectedDevice();
    switch (device) {
      case LinkCard::ConnectedDevice::E_READER_JAP:
      case LinkCard::ConnectedDevice::E_READER_USA: {
        output += "e-Reader\n\nPress A to send the loader.";

        if (Common::didPress(KEY_A, a)) {
          Common::log("Sending...\n\nPress B to cancel");
          u32 loaderSize;
          const u8* loader =
              (const u8*)gbfs_get_nth_obj(fs, 0, NULL, &loaderSize);
          auto res = linkCard->sendLoader(loader, loaderSize);
          Common::log(std::to_string((int)res));
          Common::waitForKey(KEY_DOWN);
        }

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

    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
