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
#include <functional>  // TODO: REMOVE
#include <string>      // TODO: REMOVE
#include "LinkGPIO.h"
#include "LinkSPI.h"

#define LINK_WIRELESS_PING_WAIT (160 + 68)
#define LINE_WIRELESS_LOGIN_STEPS 9

const u16 LINK_WIRELESS_LOGIN_PARTS[] = {0x494e, 0x494e, 0x544e, 0x544e, 0x4e45,
                                         0x4e45, 0x4f44, 0x4f44, 0x8001};

class LinkWireless {
 public:
  std::function<void(std::string)> debug;

  bool isActive() { return isEnabled; }

  void activate() {
    reset();
    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;
    stop();
  }

  ~LinkWireless() {
    delete linkSPI;
    delete linkGPIO;
  }

 private:
  struct LoginMemory {
    u16 previousGBAData = 0xffff;
    u16 previousAdapterData = 0xffff;
  };

  LinkSPI* linkSPI = new LinkSPI();
  LinkGPIO* linkGPIO = new LinkGPIO();
  bool isEnabled = false;

  void reset() {
    stop();
    start();
  }

  void start() {
    pingAdapter();
    linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);
    login();

    // TODO: IMPLEMENT
  }

  void stop() { linkSPI->deactivate(); }

  void pingAdapter() {
    linkGPIO->setMode(LinkGPIO::Pin::SO, LinkGPIO::Direction::OUTPUT);
    linkGPIO->setMode(LinkGPIO::Pin::SD, LinkGPIO::Direction::OUTPUT);
    linkGPIO->writePin(LinkGPIO::SD, true);
    wait(LINK_WIRELESS_PING_WAIT);
    linkGPIO->writePin(LinkGPIO::SD, false);
  }

  void login() {
    LoginMemory memory;

    debug("Sending...");

    if (!exchangeLoginPacket(LINK_WIRELESS_LOGIN_PARTS[0], 0, memory)) {
      debug("ERROR at -1");
      return;
    }

    debug("-1 ok");

    for (u32 i = 0; i < LINE_WIRELESS_LOGIN_STEPS; i++) {
      if (!exchangeLoginPacket(LINK_WIRELESS_LOGIN_PARTS[i],
                               LINK_WIRELESS_LOGIN_PARTS[i], memory)) {
        debug("ERROR AT " + std::to_string(i));
        return;
      } else {
        debug(std::to_string(i) + " ok");
      }
    }

    debug("login ok!");

    // GBA:
    //      UPPER - inverse of the Adapter's UPPER (data)
    //      LOWER - data
    // ADAPTER:
    //      UPPER - data
    //      LOWER - inverse of GBA's LOWER (data)
  }

  bool exchangeLoginPacket(u16 data,
                           u16 expectedResponse,
                           LoginMemory& memory) {
    u32 packet = buildU32(~memory.previousAdapterData, data);
    u32 response = linkSPI->transfer(packet);

    if (msB(response) != expectedResponse ||
        lsB(response) != (u16)~memory.previousGBAData)
      return false;

    memory.previousGBAData = data;
    memory.previousAdapterData = expectedResponse;

    return true;
  }

  void wait(u32 verticalLines) {
    u32 lines = 0;
    u32 vCount = REG_VCOUNT;

    while (lines < verticalLines) {
      if (REG_VCOUNT != vCount) {
        lines++;
        vCount = REG_VCOUNT;
      }
    };
  }

  u32 buildU32(u16 msB, u16 lsB) { return (msB << 16) | lsB; }
  u16 msB(u32 value) { return value >> 16; }
  u16 lsB(u32 value) { return value & 0xffff; }
};

extern LinkWireless* linkWireless;

#endif  // LINK_WIRELESS_H
