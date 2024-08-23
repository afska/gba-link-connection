// STRESS:
// This example can perform multiple stress tests.
// A) Packet loss test:
//   - It sends consecutive values in a two-player setup.
//   - The units will start running at the same time when both receive a 1.
//   - When a GBA receives something not equal to previousValue + 1, it hangs.
//   - It should continue until reaching 65534, with no packet loss.
//   - The user can purposely mess up the sync by pressing START to add lag.
// B) Packet sync test:
//   - Like (A), but using synchronous transfers.
//   - The test will ensure the remote counters match local counters.
// L) Measure ping latency:
//   - Measures how much time it takes to receive a packet from the other node.
// R) Measure ping-pong latency:
//   - Like (L), but adding a validation response and adding that time.

#include "main.h"
#include <string>
#include "../../_lib/interrupt.h"

#define FINAL_VALUE 65534

void test(bool withSync);
void measureLatency(bool withPong);
void forceSync();
void log(std::string text);
void waitFor(u16 key);
void wait(u32 verticalLines);
bool needsReset();
void profileStart();
u32 profileStop();
u32 toMs(u32 cycles);

#ifndef USE_LINK_UNIVERSAL
LinkCable* linkCable = new LinkCable();
LinkCable* linkConnection = linkCable;
#endif
#ifdef USE_LINK_UNIVERSAL
LinkUniversal* linkUniversal =
    new LinkUniversal(LinkUniversal::Protocol::AUTODETECT,
                      "LinkUniversal",
                      (LinkUniversal::CableOptions){
                          .baudRate = LinkCable::BAUD_RATE_1,
                          .timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                          .interval = LINK_CABLE_DEFAULT_INTERVAL,
                          .sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID},
                      (LinkUniversal::WirelessOptions){
                          .retransmission = true,
                          .maxPlayers = 2,
                          .timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                          .interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                          .sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID,
                          .asyncACKTimerId = 0},
                      __qran_seed);
LinkUniversal* linkConnection = linkUniversal;
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
  interrupt_set_handler(INTR_TIMER0, LINK_UNIVERSAL_ISR_ACK_TIMER);
  interrupt_enable(INTR_TIMER0);
#endif
}

int main() {
  init();

  while (true) {
#ifndef USE_LINK_UNIVERSAL
    std::string output = "LinkCable_stress (v7.0.0)\n\n";
#endif
#ifdef USE_LINK_UNIVERSAL
    std::string output = "LinkUniversal_stress (v7.0.0)\n\n";
#endif

    linkConnection->deactivate();

    output +=
        "A: Test packet loss\nB: Test packet sync\nL: Measure ping latency\nR: "
        "Measure ping-pong latency\n\nLEFT: t=100\nRIGHT: t=25\nDOWN: "
        "t=200\nUP: t=10\nSTART: Add lag\nSELECT: Reset ";
    log(output);

    waitFor(KEY_A | KEY_B | KEY_L | KEY_R);
    u16 initialKeys = ~REG_KEYS & KEY_ANY;

    u32 interval = 50;
    if (initialKeys & KEY_LEFT)
      interval = 100;
    if (initialKeys & KEY_RIGHT)
      interval = 25;
    if (initialKeys & KEY_DOWN)
      interval = 200;
    if (initialKeys & KEY_UP)
      interval = 10;
#ifndef USE_LINK_UNIVERSAL
    linkConnection->config.interval = interval;
#endif
#ifdef USE_LINK_UNIVERSAL
    linkConnection->linkCable->config.interval = interval;
    linkConnection->linkWireless->config.interval = interval;
#endif

    linkConnection->activate();

    if (initialKeys & KEY_A)
      test(false);
    else if (initialKeys & KEY_B)
      test(true);
    else if (initialKeys & KEY_L)
      measureLatency(false);
    else if (initialKeys & KEY_R)
      measureLatency(true);
  }

  return 0;
}

void test(bool withSync) {
  u16 localCounter = 0;
  u16 expectedCounter = 0;
  bool error = false;
  u16 receivedRemoteCounter = 0;

  log("Waiting for data...");

  while (true) {
    if (needsReset())
      return;

    linkConnection->sync();
    auto playerCount = linkConnection->playerCount();

    std::string output = "";

    if (linkConnection->isConnected() && playerCount == 2) {
      u16 keys = ~REG_KEYS & KEY_ANY;
      if (keys & KEY_START) {
        log("Lagging...");
        wait(1500);
      }

      auto currentPlayerId = linkConnection->currentPlayerId();
      auto remotePlayerId = !currentPlayerId;

      if (localCounter < FINAL_VALUE) {
        localCounter++;
        linkConnection->send(localCounter);
      }

      if (localCounter == 1 || withSync) {
        while (linkConnection->peek(remotePlayerId) != localCounter) {
          if (!linkConnection->waitFor(remotePlayerId, needsReset))
            return;
        }
      }

      while (linkConnection->canRead(remotePlayerId) &&
             (!withSync || expectedCounter + 1 == localCounter)) {
        expectedCounter++;
        u16 message = linkConnection->read(remotePlayerId);

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

      if (error) {
        output += "ERROR!\nExpected " + std::to_string(expectedCounter) +
                  " but got " + std::to_string(receivedRemoteCounter) + "\n\n";
        if (linkConnection->canRead(remotePlayerId)) {
          output += "Remaining packets: |";
          while (linkConnection->canRead(remotePlayerId))
            output +=
                std::to_string(linkConnection->read(remotePlayerId)) + "| ";
          output += "\n\n";
        }
      }
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
        if (needsReset())
          return;
    } else if (localCounter == FINAL_VALUE && expectedCounter == FINAL_VALUE) {
      log("Test passed!");
      while (true)
        if (needsReset())
          return;
    }
  }
}

void measureLatency(bool withPong) {
  log("Waiting for data...");

  bool didInitialize = false;
  u32 counter = 0;
  u32 samples = 0;
  u32 totalMs = 0;

  while (true) {
    if (needsReset())
      return;

    linkConnection->sync();
    auto playerCount = linkConnection->playerCount();

    if (linkConnection->isConnected() && playerCount == 2) {
      auto currentPlayerId = linkConnection->currentPlayerId();
      auto remotePlayerId = !currentPlayerId;

      if (!didInitialize) {
        counter = 11 + linkConnection->currentPlayerId() * 10;
        didInitialize = true;
      }

      forceSync();

      u32 sentPacket = ++counter;

      profileStart();
      linkConnection->send(sentPacket);
      if (!linkConnection->waitFor(remotePlayerId, needsReset)) {
        log("No response! (1) Press DOWN");
        profileStop();
        waitFor(KEY_DOWN);
        return;
      }
      u16 receivedPacket = linkConnection->read(remotePlayerId);
      if (withPong) {
        linkConnection->send(receivedPacket);
        if (!linkConnection->waitFor(remotePlayerId, needsReset)) {
          log("No response! (2) Press DOWN");
          profileStop();
          waitFor(KEY_DOWN);
          return;
        }
        u16 validation = linkConnection->read(remotePlayerId);
        if (validation != sentPacket) {
          log("Invalid response! Press DOWN\n  value = " +
              std::to_string(validation) +
              "\n  expected = " + std::to_string(sentPacket));
          profileStop();
          waitFor(KEY_DOWN);
          return;
        }
      }
      u32 elapsedCycles = profileStop();

      u32 elapsedMilliseconds = toMs(elapsedCycles);
      samples++;
      totalMs += elapsedMilliseconds;
      u32 average = Div(totalMs, samples);

      std::string output = "Ping latency: \n  " +
                           std::to_string(elapsedCycles) + " cycles\n  " +
                           std::to_string(elapsedMilliseconds) + " ms\n  " +
                           std::to_string(average) + " ms avg" +
                           "\nValue sent:\n  " + std::to_string(sentPacket);
      VBlankIntrWait();
      log(output);
    } else {
      VBlankIntrWait();
      log("Waiting...");
    }
  }
}

void forceSync() {
  auto remotePlayerId = !linkConnection->currentPlayerId();

  linkConnection->send(10);
  while (linkConnection->isConnected() && !needsReset() &&
         linkConnection->peek(remotePlayerId) != 10)
    linkConnection->waitFor(remotePlayerId);
  linkConnection->read(remotePlayerId);
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

bool needsReset() {
  u16 keys = ~REG_KEYS & KEY_ANY;
  return keys & KEY_SELECT;
}

void profileStart() {
  REG_TM1CNT_L = 0;
  REG_TM2CNT_L = 0;

  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  REG_TM2CNT_H = TM_ENABLE | TM_CASCADE;
  REG_TM1CNT_H = TM_ENABLE | TM_FREQ_1;
}

u32 profileStop() {
  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  return (REG_TM1CNT_L | (REG_TM2CNT_L << 16));
}

u32 toMs(u32 cycles) {
  // CPU Frequency * time per frame = cycles per frame
  // 16780000 * (1/60) ~= 279666
  return (cycles * 1000) / (279666 * 60);
}
