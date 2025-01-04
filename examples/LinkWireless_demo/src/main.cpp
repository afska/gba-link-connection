// (0) Include the header
#include "../../../lib/LinkWireless.hpp"

#include <tonc.h>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "../../_lib/interrupt.h"

#ifdef PROFILING_ENABLED
#include <regex>
#endif

#define CHECK_ERRORS(MESSAGE)                                             \
  if ((lastError = linkWireless->getLastError()) ||                       \
      linkWireless->getState() == LinkWireless::State::NEEDS_RESET) {     \
    log(std::string(MESSAGE) + " (" + std::to_string(lastError) + ") [" + \
        std::to_string(linkWireless->getState()) + "]");                  \
    hang();                                                               \
    linkWireless->activate();                                             \
    return;                                                               \
  }

void activate();
void serve();
void connect();
void messageLoop();
void log(std::string text);
void waitFor(u16 key);
bool didPress(u16 key, bool& pressed);
void hang();

LinkWireless::Error lastError;
LinkWireless* linkWireless = nullptr;
bool forwarding, retransmission;
u32 maxPlayers;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  std::string buildSettings = "";
#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM
  buildSettings += " + irq_iwram\n";
#endif
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  buildSettings += " + irq_nested\n";
#endif
#ifdef LINK_WIRELESS_USE_SEND_RECEIVE_LATCH
  buildSettings += " + s/r_latch\n";
#endif
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
  buildSettings += " + 2players\n";
#endif
#ifdef PROFILING_ENABLED
  buildSettings += " + profiler\n";
#endif

start:
  // Options
  log("LinkWireless_demo (v7.1.0)\n" + buildSettings +
      "\n"
      "Press A to start\n\n"
      "hold LEFT on start:\n -> disable forwarding\n\n"
      "hold UP on start:\n -> disable retransmission\n\n"
      "hold RIGHT on start:\n -> restore from multiboot\n -> high "
      "timeout\n\n"
      "hold B on start:\n -> set 2 players");
  waitFor(KEY_A);
  u16 initialKeys = ~REG_KEYS & KEY_ANY;
  forwarding = !(initialKeys & KEY_LEFT);
  retransmission = !(initialKeys & KEY_UP);
  maxPlayers = (initialKeys & KEY_B) ? 2 : LINK_WIRELESS_MAX_PLAYERS;
  bool isRestoringFromMultiboot = initialKeys & KEY_RIGHT;

  // (1) Create a LinkWireless instance
  linkWireless = new LinkWireless(
      forwarding, retransmission, maxPlayers,
      isRestoringFromMultiboot ? 1000 : LINK_WIRELESS_DEFAULT_TIMEOUT,
      LINK_WIRELESS_DEFAULT_INTERVAL, LINK_WIRELESS_DEFAULT_SEND_TIMER_ID);
  // linkWireless->debug = [](std::string str) { log(str); };

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_WIRELESS_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_WIRELESS_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_WIRELESS_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // (3) Initialize the library
  if (isRestoringFromMultiboot) {
    // Restore from multiboot
    bool success = linkWireless->restoreFromMultiboot();
    if (!success) {
      log("Multiboot restoration failed!");
      hang();
    }
  } else {
    // Normal initialization
    linkWireless->activate();
  }

  bool activating = false;
  bool serving = false;
  bool connecting = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // If a session is already active (multiboot), go straight the message loop
    if (linkWireless->isSessionActive()) {
      messageLoop();
      VBlankIntrWait();
      continue;
    }

    // Menu
    log(std::string("") +
        "L = Serve\nR = Connect\n\n (DOWN = ok)\n "
        "(SELECT = cancel)\n (START = activate)\n\n-> forwarding: " +
        (forwarding ? "ON" : "OFF") +
        "\n-> retransmission: " + (retransmission ? "ON" : "OFF") +
        "\n-> max players: " + std::to_string(maxPlayers) +
        "\n-> timeout: " + std::to_string(linkWireless->config.timeout));

    // SELECT = back
    if (keys & KEY_SELECT) {
      linkWireless->deactivate();
      interrupt_disable(INTR_VBLANK);
      interrupt_disable(INTR_SERIAL);
      interrupt_disable(INTR_TIMER3);
      interrupt_disable(INTR_TIMER0);
      delete linkWireless;
      linkWireless = nullptr;
      goto start;
    }

    // START = Activate
    if (didPress(KEY_START, activating))
      activate();

    // L = Serve
    if (didPress(KEY_L, serving))
      serve();

    // R = Connect
    if (didPress(KEY_R, connecting))
      connect();

    VBlankIntrWait();
  }

  return 0;
}

void activate() {
  log("Trying...");

  if (linkWireless->activate())
    log("Activated!");
  else
    log("Activation failed! :(");

  hang();
}

void serve() {
  log("Serving...");

  // (4) Start a server
  linkWireless->serve("LinkWireless", "Demo");
  CHECK_ERRORS("Serve failed :(")

  log("Listening...");

  do {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled!");
      linkWireless->activate();
      hang();
      return;
    }
  } while (linkWireless->getState() == LinkWireless::State::SERVING &&
           !linkWireless->isConnected());
  CHECK_ERRORS("Accept failed :(")

  log("Connection accepted!");

  messageLoop();
}

void connect() {
  u32 dotsCount, timer = 0;
  std::function<void()> animate = [&dotsCount, &timer]() {
    if (timer++ % 10 == 0)
      dotsCount = 1 + (dotsCount) % 3;

    std::string dots = "";
    for (u32 i = 0; i < dotsCount; i++)
      dots += ".";
    log("Searching" + dots);
  };

  // (5) Connect to a server
  LinkWireless::Server servers[LINK_WIRELESS_MAX_SERVERS];
  linkWireless->getServers(servers, animate);
  CHECK_ERRORS("Search failed :(")

  if (servers[0].id == LINK_WIRELESS_END) {
    log("Nothing found :(");
    hang();
    return;
  } else {
    std::string str = "Press START to connect\n(first ID will be used)\n\n";
    for (u32 i = 0; i < 3; i++) {
      auto server = servers[i];
      if (server.id == LINK_WIRELESS_END)
        break;

      str +=
          std::to_string(server.id) +
          (server.isFull() ? " [full]"
                           : " [" + std::to_string(server.currentPlayerCount) +
                                 " online]") +
          "\n";
      str += " -> gameID: " + std::to_string(server.gameId) + "\n";
      if (std::strlen(server.gameName) > 0)
        str += " -> game: " + std::string(server.gameName) + "\n";
      if (std::strlen(server.userName) > 0)
        str += " -> user: " + std::string(server.userName) + "\n";
      str += "\n";
    }
    log(str);
  }

  waitFor(KEY_START | KEY_SELECT);
  if ((~REG_KEYS & KEY_ANY) & KEY_SELECT) {
    linkWireless->activate();
    return;
  }

  linkWireless->connect(servers[0].id);
  CHECK_ERRORS("Connect failed 1 :(")

  while (linkWireless->getState() == LinkWireless::State::CONNECTING) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled!");
      linkWireless->activate();
      hang();
      return;
    }

    linkWireless->keepConnecting();
    CHECK_ERRORS("Connect failed 2 :(")
  }

  log("Connected! " + std::to_string(linkWireless->currentPlayerId()) + "\n" +
      "Waiting for server...");

  while (linkWireless->getState() == LinkWireless::State::CONNECTED &&
         !linkWireless->isConnected()) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled!");
      linkWireless->activate();
      hang();
      return;
    }

    VBlankIntrWait();
  }
  CHECK_ERRORS("Connect failed 3 :(")

  messageLoop();
}

void messageLoop() {
  // Each player starts counting from a different value:
  // 1, 11, 21, 31, 41
  std::vector<u16> counters;
  for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
    counters.push_back(1 + i * 10);

  bool sending = false;
  bool altView = false;
  bool switching = false;

#ifndef PROFILING_ENABLED
  u32 lostPackets = 0;
  u32 lastLostPacketPlayerId = 0;
  u32 lastLostPacketExpected = 0;
  u32 lastLostPacketReceived = 0;
  u32 lastLostPacketReceivedPacketId = 0;
#endif

  while (true) {
    CHECK_ERRORS("Error :(")
    u16 keys = ~REG_KEYS & KEY_ANY;

    // (6) Send data
    if ((keys & KEY_B) || (!sending && (keys & KEY_A))) {
      bool doubleSend = false;
      sending = true;

    again:
      u16 newValue = counters[linkWireless->currentPlayerId()] + 1;
      bool success = linkWireless->send(newValue);

#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
      linkWireless->QUICK_SEND = newValue % 32;
#endif

      if (success) {
        counters[linkWireless->currentPlayerId()] = newValue;
      } else {
        if (linkWireless->getLastError(false) == LinkWireless::BUFFER_IS_FULL) {
          linkWireless->getLastError();
          goto sendEnd;
        }
        CHECK_ERRORS("Send failed :(")
      }

      if (!doubleSend && (keys & KEY_LEFT)) {
        doubleSend = true;
        goto again;
      }
    }
  sendEnd:
    if (sending && (!(keys & KEY_A)))
      sending = false;

    // (7) Receive data
    LinkWireless::Message messages[LINK_WIRELESS_QUEUE_SIZE];
    linkWireless->receive(messages);
    for (u32 i = 0; i < LINK_WIRELESS_QUEUE_SIZE; i++) {
      auto message = messages[i];
      if (message.packetId == LINK_WIRELESS_END)
        break;

#ifndef PROFILING_ENABLED
      u32 expected = counters[message.playerId] + 1;
#endif

      counters[message.playerId] = message.data;

#ifndef PROFILING_ENABLED
      // Check for packet loss
      if (altView && message.data != expected) {
        lostPackets++;
        lastLostPacketPlayerId = message.playerId;
        lastLostPacketExpected = expected;
        lastLostPacketReceived = message.data;
        lastLostPacketReceivedPacketId = message.packetId;
      }
#endif
    }

    // (8) Disconnect
    if ((keys & KEY_SELECT)) {
      linkWireless->activate();
      return;
    }

    // Packet loss check setting
    if (didPress(KEY_UP, switching)) {
#ifdef PROFILING_ENABLED
      // In the profiler ROM, pressing UP will update the broadcast data
      if (linkWireless->getState() == LinkWireless::State::SERVING &&
          !(keys & KEY_START)) {
        linkWireless->serve("LinkWireless",
                            ("N = " + std::to_string(counters[0])).c_str(),
                            counters[0]);
        if (linkWireless->getLastError() ==
            LinkWireless::Error::BUSY_TRY_AGAIN) {
          log("Busy! Can't update.");
          waitFor(KEY_DOWN);
        }
      }

      // In the profiler ROM, pressing START+UP will close the server
      if (linkWireless->getState() == LinkWireless::State::SERVING &&
          !linkWireless->isServerClosed() && (keys & KEY_START)) {
        if (linkWireless->closeServer()) {
          log("Server closed!");
          waitFor(KEY_DOWN);
        } else if (linkWireless->getLastError() ==
                   LinkWireless::Error::BUSY_TRY_AGAIN) {
          log("Busy! Can't close.");
          waitFor(KEY_DOWN);
        }
      }
#endif

      altView = !altView;
#ifndef PROFILING_ENABLED
      if (!altView) {
        lostPackets = 0;
        lastLostPacketPlayerId = 0;
        lastLostPacketExpected = 0;
        lastLostPacketReceived = 0;
      }
#endif
    }

    // Normal output
    std::string output =
        "Player #" + std::to_string(linkWireless->currentPlayerId()) + " (" +
        std::to_string(linkWireless->playerCount()) + " total)" +
        "\n\n(press A to increment counter)\n(hold B to do it "
        "continuously)\n(hold LEFT for double send)\n\nPacket loss check: " +
        (altView ? "ON" : "OFF") + "\n(switch with UP)\n\n";

#ifdef PROFILING_ENABLED
    output = std::regex_replace(output, std::regex("Packet loss check"),
                                "Show profiler");
#endif

    for (u32 i = 0; i < linkWireless->playerCount(); i++) {
      output +=
          "p" + std::to_string(i) + ": " + std::to_string(counters[i]) + "\n";
    }

// Debug output
#ifdef LINK_WIRELESS_TWO_PLAYERS_ONLY
    output += "\n>> " + std::to_string(linkWireless->QUICK_SEND);
    output += "\n<< " + std::to_string(linkWireless->QUICK_RECEIVE) + "\n";
#endif

    output += "\n_buffer: " + std::to_string(linkWireless->_getPendingCount());
    if (retransmission && !altView) {
      output +=
          "\n_lastPkgId: " + std::to_string(linkWireless->_lastPacketId());
      output += "\n_nextPndngPkgId: " +
                std::to_string(linkWireless->_nextPendingPacketId());
      if (linkWireless->currentPlayerId() == 0) {
        output += "\n_lastConfFromC1: " +
                  std::to_string(linkWireless->_lastConfirmationFromClient1());
        output += "\n_lastPkgIdFromC1: " +
                  std::to_string(linkWireless->_lastPacketIdFromClient1());
      } else {
        output += "\n_lastConfFromSrv: " +
                  std::to_string(linkWireless->_lastConfirmationFromServer());
        output += "\n_lastPkgIdFromSrv: " +
                  std::to_string(linkWireless->_lastPacketIdFromServer());
      }
    }
    if (altView) {
#ifdef PROFILING_ENABLED
      output += "\n_onVBlank: " + std::to_string(linkWireless->lastVBlankTime);
      output += "\n_onSerial: " + std::to_string(linkWireless->lastSerialTime);
      output += "\n_onTimer: " + std::to_string(linkWireless->lastTimerTime);
      output +=
          "\n_serialIRQs: " + std::to_string(linkWireless->lastFrameSerialIRQs);
      output +=
          "\n_timerIRQs: " + std::to_string(linkWireless->lastFrameTimerIRQs);
      output +=
          "\n_ms: " +
          std::to_string(linkWireless->toMs(
              linkWireless->lastVBlankTime +
              linkWireless->lastSerialTime * linkWireless->lastFrameSerialIRQs +
              linkWireless->lastTimerTime * linkWireless->lastFrameTimerIRQs));
#else
      if (lostPackets > 0) {
        output += "\n\n_lostPackets: " + std::to_string(lostPackets) + "\n";
        output += "_last: (" + std::to_string(lastLostPacketPlayerId) + ":" +
                  std::to_string(lastLostPacketReceivedPacketId) + ") " +
                  std::to_string(lastLostPacketReceived) + " [vs " +
                  std::to_string(lastLostPacketExpected) + "]";
      }
#endif
    }

    // Test lag
    if (keys & KEY_DOWN)
      Link::wait(9000);

    // Print
    VBlankIntrWait();
    log(output);
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

void hang() {
  waitFor(KEY_DOWN);
}
