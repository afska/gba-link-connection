// (0) Include the header
#include "../../../lib/LinkRawCable.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

inline void VBLANK() {}

// (1) Create a LinkRawCable instance
LinkRawCable* linkRawCable = new LinkRawCable();

void init() {
  Common::initTTE();

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_RAW_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
}

int main() {
  init();

  bool firstTransfer = false;
  bool async = false;
  u32 counter = 0;
  u16 prevKeys = 0;

  while (true) {
    std::string output = "LinkRawCable_demo (v7.1.0)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkRawCable->isActive()) {
      firstTransfer = true;
      output += "START: Start MultiPlay mode\n";
      output += "\n(stop: press L+R)\n";
      output += "(hold A on start for async)\n";

      if ((keys & KEY_START) | (keys & KEY_SELECT)) {
        // (3) Initialize the library
        linkRawCable->activate();
        if (keys & KEY_A)
          async = true;
      }
    } else {
      // Title
      auto modeName = linkRawCable->isMaster() ? "[master]" : "[slave]";
      output += std::string(modeName) + "\n\n";
      output +=
          "isReady() = " + std::to_string(linkRawCable->isReady()) + "\n\n";
      if (firstTransfer) {
        Common::log(output + "Waiting...");
        firstTransfer = false;
      }

      if (!async) {
        // (4)/(5) Exchange 16-bit data with the connected consoles
        if (prevKeys == 0 && keys != 0 && linkRawCable->isReady()) {
          counter++;
          Common::log(output + "...");
          LinkRawCable::Response response =
              linkRawCable->transfer(counter, []() {
                u16 keys = ~REG_KEYS & KEY_ANY;
                return (keys & KEY_L) && (keys & KEY_R);
              });
          Common::log(output + ">> " + std::to_string(counter));
          Link::wait(228 * 60);
          Common::log(output + "<< [" + std::to_string(response.data[0]) + "," +
                      std::to_string(response.data[1]) + "," +
                      std::to_string(response.data[2]) + "," +
                      std::to_string(response.data[3]) + "]\n" +
                      "_pID: " + std::to_string(response.playerId));
          Link::wait(228 * 60);
        }
      } else {
        // (6) Exchange data asynchronously
        if (prevKeys == 0 && keys != 0 && linkRawCable->isReady() &&
            linkRawCable->getAsyncState() == LinkRawCable::AsyncState::IDLE) {
          counter++;
          linkRawCable->transferAsync(counter);
          Common::log(output + ">> " + std::to_string(counter));
          Link::wait(228 * 60);
        }
        if (linkRawCable->getAsyncState() == LinkRawCable::AsyncState::READY) {
          LinkRawCable::Response response = linkRawCable->getAsyncData();
          Common::log(output + "<< [" + std::to_string(response.data[0]) + "," +
                      std::to_string(response.data[1]) + "," +
                      std::to_string(response.data[2]) + "," +
                      std::to_string(response.data[3]) + "]\n" +
                      "_pID: " + std::to_string(response.playerId));
          Link::wait(228 * 60);
        }
      }

      // Cancel
      if ((keys & KEY_L) && (keys & KEY_R)) {
        linkRawCable->deactivate();
        async = false;
        counter = 0;
      }
    }

    // Print
    VBlankIntrWait();
    Common::log(output);
    prevKeys = keys;
  }

  return 0;
}
