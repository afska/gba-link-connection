#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// (0) Include the header
#include "../../../lib/LinkRawCable.hpp"

void log(std::string text);
void wait(u32 verticalLines);
inline void VBLANK() {}

// (1) Create a LinkRawCable instance
LinkRawCable* linkRawCable = new LinkRawCable();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

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
    std::string output = "";
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
        log(output + "Waiting...");
        firstTransfer = false;
      }

      if (!async) {
        // (4)/(5) Exchange 32-bit data with the other end
        if (prevKeys == 0 && keys != 0 && linkRawCable->isReady()) {
          counter++;
          log(output + "...");
          LinkRawCable::Response response =
              linkRawCable->transfer(counter, []() {
                u16 keys = ~REG_KEYS & KEY_ANY;
                return (keys & KEY_L) && (keys & KEY_R);
              });
          log(output + ">> " + std::to_string(counter));
          wait(228 * 60);
          log(output + "<< [" + std::to_string(response.data[0]) + "," +
              std::to_string(response.data[1]) + "," +
              std::to_string(response.data[2]) + "," +
              std::to_string(response.data[3]) + "]\n" +
              "_pID: " + std::to_string(response.playerId));
          wait(228 * 60);
        }
      } else {
        // (6) Exchange data asynchronously
        if (prevKeys == 0 && keys != 0 && linkRawCable->isReady() &&
            linkRawCable->getAsyncState() == LinkRawCable::AsyncState::IDLE) {
          counter++;
          linkRawCable->transferAsync(counter);
          log(output + ">> " + std::to_string(counter));
          wait(228 * 60);
        }
        if (linkRawCable->getAsyncState() == LinkRawCable::AsyncState::READY) {
          LinkRawCable::Response response = linkRawCable->getAsyncData();
          log(output + "<< [" + std::to_string(response.data[0]) + "," +
              std::to_string(response.data[1]) + "," +
              std::to_string(response.data[2]) + "," +
              std::to_string(response.data[3]) + "]\n" +
              "_pID: " + std::to_string(response.playerId));
          wait(228 * 60);
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
    log(output);
    prevKeys = keys;
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