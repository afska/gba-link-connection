// (0) Include the header
#include "../../../lib/LinkSPI.hpp"

#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

inline void VBLANK() {}

// (1) Create a LinkSPI instance
LinkSPI* linkSPI = new LinkSPI();

void init() {
  Common::initTTE();

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_add(INTR_VBLANK, VBLANK);
  interrupt_add(INTR_SERIAL, LINK_SPI_ISR_SERIAL);
}

int main() {
  init();

  bool firstTransfer = false;
  bool async = false;
  u32 counter = 0;

  while (true) {
    std::string output = "LinkSPI_demo (v8.0.2)\n\n";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!linkSPI->isActive()) {
      firstTransfer = true;
      output += "START: Set as Master 256Kbps\n";
      output += "SELECT: Set as Slave\n";
      output += "RIGHT: Set as Master 2Mbps\n";
      output += "\n(stop: press L+R)\n";
      output += "(hold A on start for async)\n";
      output += "(hold B on start for waitMode)\n";
      output += "(hold UP for 8-bit mode)\n";
      output +=
          "\n\n\n\n\n\n\n\n[!] to test this demo...\n      "
          "...use a GBC Link Cable!";

      if ((keys & KEY_START) | (keys & KEY_SELECT) | (keys & KEY_RIGHT)) {
        // (3) Initialize the library
        linkSPI->activate((keys & KEY_START)    ? LinkSPI::Mode::MASTER_256KBPS
                          : (keys & KEY_SELECT) ? LinkSPI::Mode::SLAVE
                                                : LinkSPI::Mode::MASTER_2MBPS,
                          (keys & KEY_UP) ? LinkSPI::DataSize::SIZE_8BIT
                                          : LinkSPI::DataSize::SIZE_32BIT);
        linkSPI->setWaitModeActive(keys &
                                   KEY_B);  // see `waitMode` in README.md
        if (keys & KEY_A)
          async = true;
      }
    } else {
      // Title
      auto modeName =
          linkSPI->getMode() == LinkSPI::Mode::SLAVE ? "[slave]" : "[master]";
      output += std::string(modeName) + "\n";
      if (firstTransfer) {
        Common::log(output + "Waiting...");
        firstTransfer = false;
      }

      if (!async) {
        // (4)/(5) Exchange 32-bit data with the other end
        u32 remoteKeys = linkSPI->transfer(keys, []() {
          u16 keys = ~REG_KEYS & KEY_ANY;
          return (keys & KEY_L) && (keys & KEY_R);
        });
        output += "> " + std::to_string(keys) + "\n";
        output += "< " + std::to_string(remoteKeys) + "\n";
      } else {
        // (6) Exchange data asynchronously
        if (keys != 0 &&
            linkSPI->getAsyncState() == LinkSPI::AsyncState::IDLE) {
          counter++;
          linkSPI->transferAsync(counter, []() {
            u16 keys = ~REG_KEYS & KEY_ANY;
            return (keys & KEY_L) && (keys & KEY_R);
          });
          Common::log(output + ">> " + std::to_string(counter));
          Link::wait(228 * 60);
        }
        if (linkSPI->getAsyncState() == LinkSPI::AsyncState::READY) {
          Common::log(output + "<< " + std::to_string(linkSPI->getAsyncData()));
          Link::wait(228 * 60);
        }
      }

      // Cancel
      if ((keys & KEY_L) && (keys & KEY_R)) {
        linkSPI->deactivate();
        async = false;
        counter = 0;
      }
    }

    // Print
    VBlankIntrWait();
    Common::log(output);
  }

  return 0;
}
