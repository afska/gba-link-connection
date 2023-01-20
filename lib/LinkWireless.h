#ifndef LINK_WIRELESS_H
#define LINK_WIRELESS_H

// --------------------------------------------------------------------------
// // TODO: COMPLETE An SPI handler for the Link Port (Normal Mode, 32bits).
// --------------------------------------------------------------------------
// // TODO: Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkWireless* linkWireless = new LinkWireless();
// - 2) Initialize the library with:
//       linkWireless->activate();
// - 3) Exchange 32-bit data with the other end:
//       _________
//       // (this blocks the console indefinitely)
// - 4) Exchange data with a cancellation callback:
//       u32 data = linkGPIO->transfer(0x1234, []() {
//         u16 keys = ~REG_KEYS & KEY_ANY;
//         return keys & KEY_START;
//       });
// --------------------------------------------------------------------------
// considerations:
// - when using Normal Mode between two GBAs, use a GBC Link Cable!
// - only use the 2Mbps mode with custom hardware (very short wires)!
// --------------------------------------------------------------------------

#include <tonc_core.h>

// TODO: UPDATE
#define LINK_WIRELESS_CANCELED 0xffffffff
#define LINK_WIRELESS_RCNT_NORMAL 0
#define LINK_WIRELESS_SIOCNT_NORMAL 0
#define LINK_WIRELESS_RCNT_GENERAL_PURPOSE (1 << 15)
#define LINK_WIRELESS_SIOCNT_GENERAL_PURPOSE 0
#define LINK_WIRELESS_BIT_CLOCK 0
#define LINK_WIRELESS_BIT_CLOCK_SPEED 1
#define LINK_WIRELESS_BIT_SI 2
#define LINK_WIRELESS_BIT_SO 3
#define LINK_WIRELESS_BIT_START 7
#define LINK_WIRELESS_BIT_LENGTH 12
#define LINK_WIRELESS_BIT_IRQ 14
#define LINK_WIRELESS_SET_HIGH(REG, BIT) REG |= 1 << BIT
#define LINK_WIRELESS_SET_LOW(REG, BIT) REG &= ~(1 << BIT)

class LinkWireless {
 public:
  bool isActive() { return isEnabled; }

  void activate() {
    // TODO: IMPLEMENT
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
    // TODO: IMPLEMENT
  }

 private:
  bool isEnabled = false;
};

extern LinkWireless* linkWireless;

#endif  // LINK_WIRELESS_H
