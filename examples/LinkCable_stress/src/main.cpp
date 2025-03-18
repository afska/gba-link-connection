// STRESS:
// This example can perform multiple stress tests.
// A) Packet loss test:
//   - It sends consecutive values in a two-player setup.
//   - The units will start running at the same time when both receive a 1.
//   - When a GBA receives something not equal to previousValue + 1, it hangs.
//   - It should continue until reaching 65534, with no packet loss.
// B) Packet sync test:
//   - Like (A), but using synchronous transfers.
//   - The test will ensure the remote counters match local counters.
// L) Measure ping latency:
//   - Measures how much time it takes to receive a packet from the other node.
// R) Measure ping-pong latency:
//   - Like (L), but adding a validation response and adding that time.
// Controls:
// - The user can purposely mess up the sync by pressing START to add lag.
// - The interval can be changed mid-test with the LEFT/RIGHT keys.

#include "main.h"
#include "../../_lib/interrupt.h"

#define FINAL_VALUE 65534

void test(bool withSync);
void measureLatency(bool withPong);
void forceSync();
bool needsReset();

u32 vblankTime = 0;
u32 serialTime = 0;
u32 timerTime = 0;
u32 vblankIRQs = 0;
u32 serialIRQs = 0;
u32 timerIRQs = 0;
u32 avgTime = 0;

#ifndef USE_LINK_UNIVERSAL
LinkCable* linkCable = new LinkCable();
LinkCable* linkConnection = linkCable;
#else
LinkUniversal* linkUniversal =
    new LinkUniversal(LinkUniversal::Protocol::AUTODETECT,
                      "LinkUniversal",
                      (LinkUniversal::CableOptions){
                          .baudRate = LinkCable::BaudRate::BAUD_RATE_1,
                          .timeout = LINK_CABLE_DEFAULT_TIMEOUT,
                          .interval = LINK_CABLE_DEFAULT_INTERVAL,
                          .sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID},
                      (LinkUniversal::WirelessOptions){
                          .retransmission = true,
                          .maxPlayers = 2,
                          .timeout = LINK_WIRELESS_DEFAULT_TIMEOUT,
                          .interval = LINK_WIRELESS_DEFAULT_INTERVAL,
                          .sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID});
LinkUniversal* linkConnection = linkUniversal;
#endif

u16 getInterval() {
#ifndef USE_LINK_UNIVERSAL
  return linkConnection->config.interval;
#else
  return linkConnection->getLinkCable()->config.interval;
#endif
}

void setInterval(u16 interval) {
#ifndef USE_LINK_UNIVERSAL
  linkConnection->config.interval = interval;
  linkConnection->resetTimer();
#else
  linkConnection->getLinkCable()->config.interval = interval;
  linkConnection->getLinkWireless()->config.interval = interval;
  linkConnection->resetTimer();
#endif
}

void setUpInterrupts(bool profiler) {
  vblankIRQs = 0;
  vblankTime = 0;
  serialTime = 0;
  timerTime = 0;
  serialIRQs = 0;
  timerIRQs = 0;

#ifndef USE_LINK_UNIVERSAL
  // LinkCable
  interrupt_add(INTR_VBLANK, profiler ? []() {
    Common::profileStart();
    LINK_CABLE_ISR_VBLANK();
    vblankTime += Common::profileStop();
    vblankIRQs++;
  } : LINK_CABLE_ISR_VBLANK);
  interrupt_add(INTR_SERIAL, profiler ? []() {
    Common::profileStart();
    LINK_CABLE_ISR_SERIAL();
    serialTime += Common::profileStop();
    serialIRQs++;
  } : LINK_CABLE_ISR_SERIAL);
  interrupt_add(INTR_TIMER3, profiler ? []() {
    Common::profileStart();
    LINK_CABLE_ISR_TIMER();
    timerTime += Common::profileStop();
    timerIRQs++;
  } : LINK_CABLE_ISR_TIMER);
#else
  // LinkUniversal
  interrupt_add(INTR_VBLANK, profiler ? []() {
    Common::profileStart();
    LINK_UNIVERSAL_ISR_VBLANK();
    vblankTime += Common::profileStop();
    vblankIRQs++;
  } : LINK_UNIVERSAL_ISR_VBLANK);
  interrupt_add(INTR_SERIAL, profiler ? []() {
    Common::profileStart();
    LINK_UNIVERSAL_ISR_SERIAL();
    serialTime += Common::profileStop();
    serialIRQs++;
  } : LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_add(INTR_TIMER3, profiler ? []() {
    Common::profileStart();
    LINK_UNIVERSAL_ISR_TIMER();
    timerTime += Common::profileStop();
    timerIRQs++;
  } : LINK_UNIVERSAL_ISR_TIMER);
#endif
}

void init() {
  Common::initTTE();

  interrupt_init();
  setUpInterrupts(false);
}

int main() {
  init();

  while (true) {
#ifndef USE_LINK_UNIVERSAL
    std::string output = "LinkCable_stress (v8.0.2)\n\n";
#else
    std::string output = "LinkUniversal_stress (v8.0.2)\n\n";
    Link::randomSeed = __qran_seed;
#endif

    linkConnection->deactivate();

    output +=
        "A: Test packet loss\nB: Test packet sync\nL: Measure ping latency\nR: "
        "Measure ping-pong latency\n\nHold DOWN: Initial t=100\nHold UP: "
        "Initial t=25\n\nLEFT/RIGHT: Change t\nSTART: Add lag\nSELECT: Reset ";
    Common::log(output);

    Common::waitForKey(KEY_A | KEY_B | KEY_L | KEY_R);
    u16 initialKeys = ~REG_KEYS & KEY_ANY;

    u32 interval = 50;
    if (initialKeys & KEY_DOWN)
      interval = 100;
    if (initialKeys & KEY_UP)
      interval = 25;
    setInterval(interval);

    linkConnection->activate();

    if (initialKeys & KEY_A) {
      setUpInterrupts(true);
      test(false);
    } else if (initialKeys & KEY_B) {
      setUpInterrupts(true);
      test(true);
    } else if (initialKeys & KEY_L) {
      setUpInterrupts(false);
      measureLatency(false);
    } else if (initialKeys & KEY_R) {
      setUpInterrupts(false);
      measureLatency(true);
    }
  }

  return 0;
}

void test(bool withSync) {
  u16 localCounter = 0;
  u16 expectedCounter = 0;
  bool error = false;
  u16 receivedRemoteCounter = 0;
  bool increasingInterval = true;
  bool decreasingInterval = true;

  Common::log("Waiting for data...");

  while (true) {
    if (needsReset())
      return;

    if (vblankIRQs >= 60) {
      avgTime = (vblankTime + serialTime + timerTime) / 60;

      vblankIRQs = 0;
      vblankTime = 0;
      serialTime = 0;
      timerTime = 0;
      serialIRQs = 0;
      timerIRQs = 0;
    }

    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_START) {
      Common::log("Lagging...");
      Link::wait(1500);
    }
    if (Common::didPress(KEY_RIGHT, increasingInterval) &&
        getInterval() < 200) {
      setInterval(getInterval() + 5);
      linkConnection->resetTimer();
    }
    if (Common::didPress(KEY_LEFT, decreasingInterval) && getInterval() > 5) {
      setInterval(getInterval() - 5);
      linkConnection->resetTimer();
    }

    linkConnection->sync();
    auto playerCount = linkConnection->playerCount();

    std::string output = "";

    if (linkConnection->isConnected() && playerCount == 2) {
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
                std::to_string(expectedCounter) +
                ")\n\ninterval = " + std::to_string(getInterval()) +
                "\ncyc/frm = " + std::to_string(avgTime);
    } else {
      output += "Waiting...";
      localCounter = 0;
      expectedCounter = 0;
      error = false;
      receivedRemoteCounter = 0;
    }

    VBlankIntrWait();
    Common::log(output);

    if (error) {
      while (true)
        if (needsReset())
          return;
    } else if (localCounter == FINAL_VALUE && expectedCounter == FINAL_VALUE) {
      Common::log("Test passed!");
      while (true)
        if (needsReset())
          return;
    }
  }
}

void measureLatency(bool withPong) {
  Common::log("Waiting for data...");

  bool didInitialize = false;
  u32 counter = 0;
  u32 samples = 0;
  u32 totalMs = 0;
  bool increasingInterval = false;
  bool decreasingInterval = false;

  while (true) {
    if (needsReset())
      return;

    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_START) {
      Common::log("Lagging...");
      Link::wait(1500);
    }
    if (Common::didPress(KEY_RIGHT, increasingInterval) &&
        getInterval() < 200) {
      setInterval(getInterval() + 5);
      linkConnection->resetTimer();
      counter = samples = totalMs = 0;
    }
    if (Common::didPress(KEY_LEFT, decreasingInterval) && getInterval() > 5) {
      setInterval(getInterval() - 5);
      linkConnection->resetTimer();
      counter = samples = totalMs = 0;
    }

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

      Common::profileStart();
      linkConnection->send(sentPacket);
      if (!linkConnection->waitFor(remotePlayerId, needsReset)) {
        Common::log("No response! (1) Press DOWN");
        Common::profileStop();
        Common::waitForKey(KEY_DOWN);
        return;
      }
      u16 receivedPacket = linkConnection->read(remotePlayerId);
      if (withPong) {
        linkConnection->send(receivedPacket);
        if (!linkConnection->waitFor(remotePlayerId, needsReset)) {
          Common::log("No response! (2) Press DOWN");
          Common::profileStop();
          Common::waitForKey(KEY_DOWN);
          return;
        }
        u16 validation = linkConnection->read(remotePlayerId);
        if (validation != sentPacket) {
          Common::log("Invalid response! Press DOWN\n  value = " +
                      std::to_string(validation) +
                      "\n  expected = " + std::to_string(sentPacket));
          Common::profileStop();
          Common::waitForKey(KEY_DOWN);
          return;
        }
      }
      u32 elapsedCycles = Common::profileStop();

      u32 elapsedMilliseconds = Common::toMs(elapsedCycles);
      samples++;
      totalMs += elapsedMilliseconds;
      u32 average = Div(totalMs, samples);

      std::string output = "Ping latency: \n  " +
                           std::to_string(elapsedCycles) + " cycles\n  " +
                           std::to_string(elapsedMilliseconds) + " ms\n  " +
                           std::to_string(average) + " ms avg" +
                           "\nValue sent:\n  " + std::to_string(sentPacket) +
                           "\n\ninterval = " + std::to_string(getInterval());
      VBlankIntrWait();
      Common::log(output);
    } else {
      VBlankIntrWait();
      Common::log("Waiting...");
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

bool needsReset() {
  u16 keys = ~REG_KEYS & KEY_ANY;
  return keys & KEY_SELECT;
}
