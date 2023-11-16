#include "main.h"
#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

// STRESS:
// This example can perform multiple stress tests.
// A) Packet loss test:
//   - This example sends consecutive values in a two-player setup.
//   - The units will start running at the same time when both receive a 1.
//   - When a GBA receives something not equal to previousValue + 1, it hangs.
//   - It should continue until reaching 65534, with no packet loss.
//   - The user can purposely mess up the sync by pressing START to add lag.
// B) Packet sync test:
//   - Same as (A), but using synchronous transfers.
//   - The test will ensure the remote counters match local counters.

#define FINAL_VALUE 65534

void test(bool withSync);
void log(std::string text);
void waitFor(u16 key);
void wait(u32 verticalLines);
void resetIfNeeded();

#ifndef USE_LINK_UNIVERSAL
LinkCable* linkCable = new LinkCable();
LinkCable* link = linkCable;
#endif
#ifdef USE_LINK_UNIVERSAL
LinkUniversal* linkUniversal = new LinkUniversal();
LinkUniversal* link = linkUniversal;
#endif

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  interrupt_init();

#ifndef USE_LINK_UNIVERSAL
  // LinkCable
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#endif
#ifdef USE_LINK_UNIVERSAL
  // LinkUniversal
  interrupt_set_handler(INTR_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#endif

  link->activate();
}

int main() {
  init();

#ifndef USE_LINK_UNIVERSAL
  std::string output = "LinkCable\n\n";
#endif
#ifdef USE_LINK_UNIVERSAL
  std::string output = "LinkUniversal\n\n";
#endif

  output +=
      "A: Test packet loss\nB: Test packet sync\n\n"
      "START: Add lag\nL: Reset";
  log(output);

  waitFor(KEY_A | KEY_B);
  u16 initialKeys = ~REG_KEYS & KEY_ANY;

  if (initialKeys & KEY_A)
    test(false);
  else if (initialKeys & KEY_B)
    test(true);
  else if (initialKeys & KEY_R)
    test(true);

  return 0;
}

void test(bool withSync) {
  u16 localCounter = 0;
  u16 expectedCounter = 0;
  bool error = false;
  u16 receivedRemoteCounter = 0;

  log("Waiting for data...");

  while (true) {
    resetIfNeeded();

    link->sync();
    auto playerCount = link->playerCount();

    std::string output = "";

    if (link->isConnected() && playerCount == 2) {
      u16 keys = ~REG_KEYS & KEY_ANY;
      if (keys & KEY_START) {
        log("Lagging...");
        wait(3000);
      }

      auto currentPlayerId = link->currentPlayerId();
      auto remotePlayerId = !currentPlayerId;

      if (localCounter == 0 && link->currentPlayerId() == 0) {
        // NO$GBA 3.04 hack (not required on actual hardware)
        // The emulator incorrectly triggers the master node's serial IRQs even
        // if the slave node has SIO disabled, so depending on which console
        // started first, the initial packets might be lost. This fix forces the
        // master node to wait until the slave is actually connected.
        link->waitFor(remotePlayerId, []() {
          resetIfNeeded();
          return false;
        });
      }

      if (localCounter < FINAL_VALUE) {
        localCounter++;
        link->send(localCounter);
      }

      if (localCounter == 1 || withSync) {
        while (link->peek(remotePlayerId) != localCounter) {
          link->waitFor(remotePlayerId, []() {
            resetIfNeeded();
            return false;
          });
        }
      }

      while (link->canRead(remotePlayerId)) {
        expectedCounter++;
        u16 message = link->read(remotePlayerId);

        if (message != expectedCounter) {
          error = true;
          receivedRemoteCounter = message;
          break;
        } else if (withSync && message != localCounter) {
          error = true;
          receivedRemoteCounter = message;
          expectedCounter = localCounter;
        }
      }

      if (error)
        output += "ERROR!\nExpected " + std::to_string(expectedCounter) +
                  " but got " + std::to_string(receivedRemoteCounter) + "\n";
      output += "(" + std::to_string(localCounter) + ", " +
                std::to_string(expectedCounter) + ")\n";
    } else {
      output += "Waiting...";
      localCounter = 0;
      expectedCounter = 0;
      error = false;
      receivedRemoteCounter = 0;
    }

    VBlankIntrWait();
    log(output);

    if (error) {
      while (true)
        resetIfNeeded();
    } else if (localCounter == FINAL_VALUE && expectedCounter == FINAL_VALUE) {
      log("Test passed!");
      while (true)
        resetIfNeeded();
    }
  }
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

void resetIfNeeded() {
  u16 keys = ~REG_KEYS & KEY_ANY;
  if (keys & KEY_L) {
    link->deactivate();

    RegisterRamReset(RESET_REG | RESET_VRAM);
    SoftReset();
  }
}