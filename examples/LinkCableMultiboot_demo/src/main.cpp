// (0) Include the header
#include "../../../lib/LinkCableMultiboot.hpp"

#include <cstring>
#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"

extern "C" {
#include "../../_lib/libgbfs/gbfs.h"
}

static const GBFS_FILE* fs = find_first_gbfs_file(0);
static u32 selectedFile = 0;
static bool spi = false;

// (1) Create a LinkCableMultiboot instance
LinkCableMultiboot* linkCableMultiboot = new LinkCableMultiboot();

void selectLeft() {
  if (selectedFile == 0)
    return;
  selectedFile--;
}

void selectRight() {
  if ((int)selectedFile >= fs->dir_nmemb - 1)
    return;
  selectedFile++;
}

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

  bool left = true, right = true, a = true, b = true, l = true;

  while (true) {
    // Get selected ROM name
    char name[32];
    u32 romSize;
    const u8* romToSend =
        (const u8*)gbfs_get_nth_obj(fs, selectedFile, name, &romSize);
    for (u32 i = 0; i < 32; i++) {
      if (name[i] == '.') {
        name[i] = '\0';
        break;
      }
    }

    // Toggle mode
    if (Common::didPress(KEY_L, l))
      spi = !spi;

    // Select ROM
    if (Common::didPress(KEY_LEFT, left))
      selectLeft();
    if (Common::didPress(KEY_RIGHT, right))
      selectRight();

    // Menu
    Common::log(
        "LinkCableMultiboot_demo\n  (v8.0.3)\n\n"
        "Press A to send the ROM...\n"
        "Press B to launch the ROM...\nLEFT/RIGHT: select ROM\nL: toggle "
        "mode\n\nSelected ROM:\n  " +
        std::string(name) + "\n\nMode:\n  " +
        std::string(spi ? "SPI (GBC cable)" : "MULTI_PLAY (GBA cable)"));

    // Send ROM
    if (Common::didPress(KEY_A, a)) {
      Common::log("Sending... (SELECT to cancel)");

      // (2) Send the ROM
      auto result = linkCableMultiboot->sendRom(
          romToSend, romSize,
          []() {
            u16 keys = ~REG_KEYS & KEY_ANY;
            return keys & KEY_SELECT;
          },
          spi ? LinkCableMultiboot::TransferMode::SPI
              : LinkCableMultiboot::TransferMode::MULTI_PLAY);

      // Print results and wait
      Common::log("Result: " + std::to_string((int)result) + "\n" +
                  "Press DOWN to continue...");
      Common::waitForKey(KEY_DOWN);
    }

    // Launch ROM
    if (Common::didPress(KEY_B, b)) {
      Common::log("Launching...");
      VBlankIntrWait();

      REG_IME = 0;

      u32 fileLength;
      const u8* romToSend =
          (const u8*)gbfs_get_nth_obj(fs, selectedFile, NULL, &fileLength);

      void* EWRAM = (void*)0x02000000;
      std::memcpy(EWRAM, romToSend, fileLength);

      asm volatile(
          "mov r0, %0\n"
          "bx r0\n"
          :
          : "r"(EWRAM)
          : "r0");

      while (true)
        ;
    }

    VBlankIntrWait();
  }

  return 0;
}
