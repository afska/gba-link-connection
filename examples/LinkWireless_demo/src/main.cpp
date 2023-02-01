#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

void activate();
void serve();
void connect();
void messageLoop();
void log(std::string text);
void waitFor(u16 key);
void hang();

// (1) Create a LinkWireless instance
LinkWireless* linkWireless = new LinkWireless();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  irq_init(NULL);
  irq_add(II_VBLANK, NULL);

  // (2) Initialize the library
  linkWireless->activate();
}

int main() {
  init();

  bool activating = false;
  bool serving = false;
  bool connecting = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    log("START = Activate\nL = Serve\nR = Connect\n\n (DOWN = ok)\n (SELECT = "
        "cancel)");

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
  if (!linkWireless->serve()) {
    log("Serve failed :(");
    hang();
    return;
  }

  log("Listening...");

  do {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled");
      linkWireless->disconnect();
      hang();
      return;
    }

    if (!linkWireless->acceptConnections()) {
      log("Accept failed :(");
      hang();
      return;
    }
  } while (linkWireless->getPlayerCount() <= 1);

  log("Connection accepted!");

  messageLoop();
}

void connect() {
  log("Searching...");

  // (4) Connect to a server
  std::vector<u16> serverIds;
  if (!linkWireless->getServerIds(serverIds)) {
    log("Search failed :(");
    hang();
    return;
  }

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

  waitFor(KEY_START);

  if (!linkWireless->connect(serverIds[0])) {
    log("Connect failed :(");
    hang();
    return;
  }

  while (linkWireless->getState() == LinkWireless::State::CONNECTING) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    if (keys & KEY_SELECT) {
      log("Canceled");
      linkWireless->disconnect();
      hang();
      return;
    }

    if (!linkWireless->keepConnecting()) {
      log("Finish connection failed :(");
      hang();
      return;
    }
  }

  log("Connected! " + std::to_string(linkWireless->getPlayerId()));

  messageLoop();
}

void messageLoop() {
  u32 i = linkWireless->getPlayerId() * 10;
  bool sending = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // (5) Send data
    if (!sending && (keys & KEY_A)) {
      sending = true;
      if (!linkWireless->sendData(std::vector<u32>{i})) {
        log("Send failed :(");
        hang();
        return;
      }
      i++;
    }
    if (sending && !(keys & KEY_A))
      sending = false;

    // (6) Receive data
    std::vector<u32> receivedData = std::vector<u32>{};
    if (!linkWireless->receiveData(receivedData)) {
      log("Receive failed :(");
      hang();
      return;
    }
    if (receivedData.size() > 0) {
      std::string str = "Total: " + std::to_string(receivedData.size()) + "\n";
      for (u32& number : receivedData)
        str += std::to_string(number) + "\n";
      log(str);
    }

    if (linkWireless->getState() == LinkWireless::State::SERVING) {
      if (!linkWireless->acceptConnections()) {
        log("Accept failed :(");
        hang();
        return;
      }
    }

    // (7) Disconnect
    if ((keys & KEY_SELECT)) {
      if (!linkWireless->disconnect()) {
        log("Disconnect failed :(");
        hang();
        return;
      }
      return;
    }

    VBlankIntrWait();
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