#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

#define TRANSFERS_PER_FRAME 4

#define CHECK_ERRORS(MESSAGE)                                             \
  if ((lastError = linkWireless->getLastError())) {                       \
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
bool forwarding = true;
bool retransmission = true;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  irq_init(NULL);
  irq_add(II_VBLANK, NULL);
}

int main() {
  init();

start:
  // Options
  log("Press A to start\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nhold LEFT on start:\n -> "
      "disable forwarding\n\nhold UP on start:\n -> disable retransmission");
  waitFor(KEY_A);
  u16 initialKeys = ~REG_KEYS & KEY_ANY;
  forwarding = !(initialKeys & KEY_LEFT);
  retransmission = !(initialKeys & KEY_UP);

  // (1) Create a LinkWireless instance
  linkWireless = new LinkWireless(forwarding, retransmission);

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
  linkWireless->serve();
  CHECK_ERRORS("Serve failed :(")

  log("Listening...");

  do {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled");
      linkWireless->disconnect();
      hang();
      return;
    }

    linkWireless->acceptConnections();
    CHECK_ERRORS("Accept failed :(")
  } while (linkWireless->getPlayerCount() <= 1);

  log("Connection accepted!");

  messageLoop();
}

void connect() {
  log("Searching...");

  // (4) Connect to a server
  std::vector<u16> serverIds;
  linkWireless->getServerIds(serverIds);
  CHECK_ERRORS("Search failed :(")

  if (serverIds.size() == 0) {
    log("Nothing found :(");
    hang();
    return;
  } else {
    std::string str = "Press START to connect\n(first ID will be used)\n\n";
    for (u16& number : serverIds)
      str += std::to_string(number) + "\n";
    log(str);
  }

  waitFor(KEY_START | KEY_SELECT);
  if ((~REG_KEYS & KEY_ANY) & KEY_SELECT) {
    linkWireless->disconnect();
    return;
  }

  linkWireless->connect(serverIds[0]);
  CHECK_ERRORS("Connect failed :(")

  while (linkWireless->getState() == LinkWireless::State::CONNECTING) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled");
      linkWireless->disconnect();
      hang();
      return;
    }

    linkWireless->keepConnecting();
    CHECK_ERRORS("Finish failed :(")
  }

  log("Connected! " + std::to_string(linkWireless->getPlayerId()));

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
    u16 keys = ~REG_KEYS & KEY_ANY;

    // (5) Send data
    if (linkWireless->canSend() &&
        ((keys & KEY_B) || (!sending && (keys & KEY_A)))) {
      bool doubleSend = false;
      sending = true;

    again:
      counters[linkWireless->getPlayerId()]++;
      linkWireless->send(
          std::vector<u32>{counters[linkWireless->getPlayerId()]});
      CHECK_ERRORS("Send failed :(")

      if (!doubleSend && (keys & KEY_LEFT) && linkWireless->canSend()) {
        doubleSend = true;
        goto again;
      }
    }
    if (sending && (!(keys & KEY_A)))
      sending = false;

    // (6) Receive data
    std::vector<LinkWireless::Message> messages;
    if (retransmission) {
      // (exchanging data 4 times, just for speed purposes)
      linkWireless->receive(messages, TRANSFERS_PER_FRAME, []() {
        u16 keys = ~REG_KEYS & KEY_ANY;
        return keys & KEY_SELECT;
      });
    } else {
      // (exchanging data one time)
      linkWireless->receive(messages);
    }
    CHECK_ERRORS("Receive failed :(")
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

    // Accept new connections
    if (linkWireless->getState() == LinkWireless::State::SERVING) {
      linkWireless->acceptConnections();
      CHECK_ERRORS("Accept failed :(")
    }

    // (7) Disconnect
    if ((keys & KEY_SELECT)) {
      if (!linkWireless->disconnect()) {
        log("Disconn failed :(");
        hang();
        return;
      }
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
        "Player #" + std::to_string(linkWireless->getPlayerId()) + " (" +
        std::to_string(linkWireless->getPlayerCount()) + " total)" +
        "\n\n(press A to increment counter)\n(hold B to do it "
        "continuously)\n(hold LEFT for double send)\n\nPacket loss check: " +
        (packetLossCheck ? "ON" : "OFF") + "\n(switch with UP)\n\n";
    for (u32 i = 0; i < linkWireless->getPlayerCount(); i++) {
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