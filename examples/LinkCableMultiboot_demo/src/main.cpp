// (0) Include the header
#include "../../../lib/LinkCableMultiboot.hpp"

#include <string.h>
#include <tonc.h>
#include <string>

extern "C" {
#include "../../_lib/libgbfs/gbfs.h"
}

static const GBFS_FILE* fs = find_first_gbfs_file(0);
static u32 selectedFile = 0;

void log(std::string text);
void waitFor(u16 key);
bool didPress(u16 key, bool& pressed);

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
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  irq_init(NULL);
  irq_add(II_VBLANK, NULL);
}

int main() {
  init();

  // Ensure there are GBFS files
  if (fs == NULL) {
    log("! GBFS file not found");
    while (true)
      ;
  } else if (gbfs_get_nth_obj(fs, 0, NULL, NULL) == NULL) {
    log("! No files found (GBFS)");
    while (true)
      ;
  }

  bool left = false, right = false, a = false, b = false;

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

    // Menu
    log("LinkCableMultiboot_demo\n  (v7.0.0)\n\n"
        "Press A to send the ROM...\n"
        "Press B to launch the ROM...\nLEFT/RIGHT: select ROM\n\nSelected "
        "ROM:\n  " +
        std::string(name));

    // Select ROM
    if (didPress(KEY_LEFT, left))
      selectLeft();
    if (didPress(KEY_RIGHT, right))
      selectRight();

    // Send ROM
    if (didPress(KEY_A, a)) {
      log("Sending... (SELECT to cancel)");

      // (2) Send the ROM
      auto result = linkCableMultiboot->sendRom(romToSend, romSize, []() {
        u16 keys = ~REG_KEYS & KEY_ANY;
        return keys & KEY_SELECT;
      });

      // Print results and wait
      log("Result: " + std::to_string(result) + "\n" +
          "Press DOWN to continue...");
      waitFor(KEY_DOWN);
    }

    // Launch ROM
    if (didPress(KEY_B, b)) {
      log("Launching...");
      VBlankIntrWait();

      REG_IME = 0;

      u32 fileLength;
      const u8* romToSend =
          (const u8*)gbfs_get_nth_obj(fs, selectedFile, NULL, &fileLength);

      void* EWRAM = (void*)0x02000000;
      memcpy(EWRAM, romToSend, fileLength);

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

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}

void waitFor(u16 key) {
  u16 keys;
  do {
    keys = ~REG_KEYS & KEY_ANY;
  } while (!(keys & key));
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
