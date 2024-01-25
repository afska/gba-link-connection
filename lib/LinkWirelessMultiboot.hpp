#ifndef LINK_WIRELESS_MULTIBOOT_H
#define LINK_WIRELESS_MULTIBOOT_H

// --------------------------------------------------------------------------
// A Wireless Multiboot tool to send small ROMs from a GBA to up to 4 slaves.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWirelessMultiboot* linkWirelessMultiboot =
//         new LinkWirelessMultiboot();
// - 2) Send the ROM:
//       LinkWirelessMultiboot::Result result = linkWirelessMultiboot->sendRom(
//         romBytes, // for current ROM, use: ((const void*)MEM_EWRAM)
//         romLength, // should be multiple of 0x10
//         []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, transfer will be canceled)
//         }
//       );
//       // `result` should be LinkWirelessMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include "LinkRawWireless.hpp"

#define LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE (0x100 + 0xc0)
#define LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE (256 * 1024)
#define LINK_WIRELESS_MULTIBOOT_HEADER_SIZE 0xC0

static volatile char LINK_WIRELESS_MULTIBOOT_VERSION[] =
    "LinkWirelessMultiboot/v6.2.0";

class LinkWirelessMultiboot {
 public:
  enum Result { SUCCESS, INVALID_SIZE, CANCELED, FAILURE };

  template <typename F>
  Result sendRom(const void* rom, u32 romSize, F cancel) {
    // if (romSize < LINK_WIRELESS_MULTIBOOT_MIN_ROM_SIZE)
    //   return INVALID_SIZE;
    // if (romSize > LINK_WIRELESS_MULTIBOOT_MAX_ROM_SIZE)
    //   return INVALID_SIZE;
    // if ((romSize % 0x10) != 0)
    //   return INVALID_SIZE;

    // TODO: IMPLEMENT

    return SUCCESS;
  }

 private:
};

extern LinkWirelessMultiboot* linkWirelessMultiboot;

#endif  // LINK_WIRELESS_MULTIBOOT_H
