// (0) Include the header
#include "../../../lib/LinkCard.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

extern "C" {
#include "../../_lib/libgbfs/gbfs.h"
}

#define USA_LOADER "usa.loader"
#define JAP_LOADER "jap.loader"

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
  } else if (gbfs_get_obj(fs, USA_LOADER, NULL) == NULL) {
    Common::log("! usa.loader not found (GBFS)");
    while (true)
      ;
  } else if (gbfs_get_obj(fs, JAP_LOADER, NULL) == NULL) {
    Common::log("! jap.loader not found (GBFS)");
    while (true)
      ;
  }

  bool a = true;

  while (true) {
    std::string output = "LinkCard_demo (v8.0.3)\n\n";
    output += "Device: ";

    // (2) Probe the connected device
    u32 initialVCount = REG_VCOUNT;
    auto device = linkCard->getConnectedDevice([&initialVCount]() {
      u32 elapsed = (REG_VCOUNT - initialVCount + 228) % 228;
      return elapsed > 150;
    });

    switch (device) {
      case LinkCard::ConnectedDevice::E_READER_JAP:
      case LinkCard::ConnectedDevice::E_READER_USA: {
        output += "e-Reader\n\nPress A to send the loader.";

        if (Common::didPress(KEY_A, a)) {
          Common::log("Sending...\n\nPress B to cancel");
          auto fileName = device == LinkCard::ConnectedDevice::E_READER_JAP
                              ? JAP_LOADER
                              : USA_LOADER;

          u32 loaderSize;
          const u8* loader = (const u8*)gbfs_get_obj(fs, fileName, &loaderSize);

          // (3) Send the DLC loader program
          auto result = linkCard->sendLoader(loader, loaderSize, []() {
            u16 keys = ~REG_KEYS & KEY_ANY;
            return keys & KEY_B;
          });

          if (result == LinkCard::SendResult::SUCCESS)
            Common::log("Success! Press DOWN");
          else
            Common::log("Error " + std::to_string((int)result) +
                        "! Press DOWN");

          Common::waitForKey(KEY_DOWN);
        }

        break;
      }
      case LinkCard::ConnectedDevice::DLC_LOADER: {
        output += "DLC Loader\n\nPress A to receive a card.";

        if (Common::didPress(KEY_A, a)) {
          Common::log("Receiving...\n\nPress B to cancel");

          // (4) Receive scanned cards
          u8 card[LINK_CARD_SIZE + 1];  // +1 to interpret contents as a string
          auto result = linkCard->receiveCard(card, []() {
            u16 keys = ~REG_KEYS & KEY_ANY;
            return keys & KEY_B;
          });

          if (result == LinkCard::ReceiveResult::SUCCESS) {
            card[LINK_CARD_SIZE] = '\0';
            Common::log("Success! Press DOWN\n\n" + std::string((char*)card));
          } else
            Common::log("Error " + std::to_string((int)result) +
                        "! Press DOWN");

          Common::waitForKey(KEY_DOWN);
        }

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
