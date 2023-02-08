#include <tonc.h>
#include <functional>
#include <string>
#include "../../_lib/interrupt.h"

// (0) Include the header
#include "../../../lib/LinkWireless.h"

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
void hang();

LinkWireless::Error lastError;
LinkWireless* linkWireless = NULL;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  bool firstTime = true;

start:
  // Options
  log("Press A to start\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nhold LEFT on start:\n -> "
      "disable forwarding\n\nhold UP on start:\n -> disable retransmission");
  waitFor(KEY_A);
  u16 initialKeys = ~REG_KEYS & KEY_ANY;
  bool forwarding = !(initialKeys & KEY_LEFT);
  bool retransmission = !(initialKeys & KEY_UP);

  // (1) Create a LinkWireless instance
  linkWireless = new LinkWireless(forwarding, retransmission);

  if (firstTime) {
    // (2) Add the required interrupt service routines
    interrupt_init();
    interrupt_set_handler(INTR_VBLANK, LINK_WIRELESS_ISR_VBLANK);
    interrupt_enable(INTR_VBLANK);
    interrupt_set_handler(INTR_SERIAL, LINK_WIRELESS_ISR_SERIAL);
    interrupt_enable(INTR_SERIAL);
    interrupt_set_handler(INTR_TIMER3, LINK_WIRELESS_ISR_TIMER);
    interrupt_enable(INTR_TIMER3);
    firstTime = false;
  }

  // (2) Initialize the library
  linkWireless->activate();

  bool activating = false;
  bool serving = false;
  bool connecting = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // Menu
    log(std::string("") +
        "L = Serve\nR = Connect\n\n (DOWN = ok)\n "
        "(SELECT = cancel)\n (START = activate)\n\n-> forwarding: " +
        (forwarding ? "ON" : "OFF") + "\n" +
        "-> retransmission: " + (retransmission ? "ON" : "OFF"));

    // SELECT = back
    if (keys & KEY_SELECT) {
      linkWireless->deactivate();
      delete linkWireless;
      linkWireless = NULL;
      goto start;
    }

    // START = Activate
    if ((keys & KEY_START) && !activating) {
      activating = true;
      activate();
    }
    if (activating && !(keys & KEY_START))
      activating = false;

    // L = Serve
    if ((keys & KEY_L) && !serving) {
      serving = true;
      serve();
    }
    if (serving && !(keys & KEY_L))
      serving = false;

    // R = Connect
    if (!connecting && (keys & KEY_R)) {
      connecting = true;
      connect();
    }
    if (connecting && !(keys & KEY_R))
      connecting = false;

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

  // (3) Start a server
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

  // (4) Connect to a server
  std::vector<LinkWireless::Server> servers;
  linkWireless->getServers(servers, animate);
  CHECK_ERRORS("Search failed :(")

  if (servers.size() == 0) {
    log("Nothing found :(");
    hang();
    return;
  } else {
    std::string str = "Press START to connect\n(first ID will be used)\n\n";
    for (auto& server : servers) {
      str += std::to_string(server.id) + "\n";
      if (server.gameName.length() > 0)
        str += " -> game: " + server.gameName + "\n";
      if (server.userName.length() > 0)
        str += " -> user: " + server.userName + "\n";
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
  CHECK_ERRORS("Connect failed :(")

  while (linkWireless->getState() == LinkWireless::State::CONNECTING) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled!");
      linkWireless->activate();
      hang();
      return;
    }

    linkWireless->keepConnecting();
    CHECK_ERRORS("Finish failed :(")
  }

  log("Connected! " + std::to_string(linkWireless->currentPlayerId()));

  messageLoop();
}

void messageLoop() {
  // Each player starts counting from a different value:
  // 1, 11, 21, 31, 41
  std::vector<u32> counters;
  for (u32 i = 0; i < LINK_WIRELESS_MAX_PLAYERS; i++)
    counters.push_back(1 + i * 10);

  bool sending = false;
  bool packetLossCheck = false;
  bool switching = false;

  u32 lostPackets = 0;
  u32 lastLostPacketPlayerId = 0;
  u32 lastLostPacketExpected = 0;
  u32 lastLostPacketReceived = 0;

  while (true) {
    CHECK_ERRORS("Error :(")
    u16 keys = ~REG_KEYS & KEY_ANY;

    // (5) Send data
    if (linkWireless->canSend() &&
        ((keys & KEY_B) || (!sending && (keys & KEY_A)))) {
      bool doubleSend = false;
      sending = true;

    again:
      counters[linkWireless->currentPlayerId()]++;
      linkWireless->send(
          std::vector<u32>{counters[linkWireless->currentPlayerId()]});
      CHECK_ERRORS("Send failed :(")

      if (!doubleSend && (keys & KEY_LEFT) && linkWireless->canSend()) {
        doubleSend = true;
        goto again;
      }
    }
    if (sending && (!(keys & KEY_A)))
      sending = false;

    // (6) Receive data
    std::vector<LinkWireless::Message> messages = linkWireless->receive();
    if (messages.size() > 0) {
      for (auto& message : messages) {
        u32 expected = counters[message.playerId] + 1;

        counters[message.playerId] = message.data[0];

        // Check for packet loss
        if (packetLossCheck && message.data[0] != expected) {
          lostPackets++;
          lastLostPacketPlayerId = message.playerId;
          lastLostPacketExpected = expected;
          lastLostPacketReceived = message.data[0];
        }
      }
    }

    // (7) Disconnect
    if ((keys & KEY_SELECT)) {
      linkWireless->activate();
      return;
    }

    // Packet loss check setting
    if (!switching && (keys & KEY_UP)) {
      switching = true;
      packetLossCheck = !packetLossCheck;
      if (!packetLossCheck) {
        lostPackets = 0;
        lastLostPacketPlayerId = 0;
        lastLostPacketExpected = 0;
        lastLostPacketReceived = 0;
      }
    }
    if (switching && (!(keys & KEY_UP)))
      switching = false;

    std::string output =
        "Player #" + std::to_string(linkWireless->currentPlayerId()) + " (" +
        std::to_string(linkWireless->playerCount()) + " total)" +
        "\n\n(press A to increment counter)\n(hold B to do it "
        "continuously)\n(hold LEFT for double send)\n\nPacket loss check: " +
        (packetLossCheck ? "ON" : "OFF") + "\n(switch with UP)\n\n";
    for (u32 i = 0; i < linkWireless->playerCount(); i++) {
      output +=
          "p" + std::to_string(i) + ": " + std::to_string(counters[i]) + "\n";
    }
    output += "\n_buffer: " + std::to_string(linkWireless->getPendingCount());
    if (packetLossCheck && lostPackets > 0) {
      output += "\n\n_lostPackets: " + std::to_string(lostPackets) + "\n";
      output += "_last: (" + std::to_string(lastLostPacketPlayerId) + ") " +
                std::to_string(lastLostPacketReceived) + " [vs " +
                std::to_string(lastLostPacketExpected) + "]";
    }

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

void hang() {
  waitFor(KEY_DOWN);
}