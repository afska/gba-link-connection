#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

void activate();
void host();
void connect();
void messageLoop(bool acceptNewClients);
void log(std::string text);
void waitFor(u16 key);
void vBlankWait();
void hang();

// (1) Create a LinkWireless instance
LinkWireless* linkWireless = new LinkWireless();

void init() {
  linkWireless->debug = [](std::string text) { log(text); };  // TODO: REMOVE

  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  bool activating = false;
  bool hosting = false;
  bool connecting = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    log("START = Activate\nL = Host\nR = Connect");

    // START = Activate
    if ((keys & KEY_START) && !activating) {
      activating = true;
      activate();
    }
    if (activating && !(keys & KEY_START))
      activating = false;

    // L = Host
    if ((keys & KEY_L) && !hosting) {
      hosting = true;
      host();
    }
    if (hosting && !(keys & KEY_L))
      hosting = false;

    // R = Connect
    if (!connecting && (keys & KEY_R)) {
      connecting = true;
      connect();
    }
    if (connecting && !(keys & KEY_R))
      connecting = false;

    vBlankWait();
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

void host() {
  log("Hosting...");

  if (!linkWireless->host(std::vector<u32>{0x0c020002, 0x00005ce1, 0x00000000,
                                           0x09000040, 0xc1cfc8cd,
                                           0x00ffccbb})) {
    log("Hosting failed :(");
    hang();
    return;
  }

  log("Listening...");

  LinkWireless::ClientIdResponses response;
  do {
    response = linkWireless->acceptConnection();
    if (!response.success) {
      log("Accept failed :(");
      hang();
      return;
    }
  } while (response.clientIds[0] == 0);

  log("Connected! " + std::to_string(response.clientIds[0]));

  messageLoop(true);
}

void connect() {
  log("Searching...");

  std::vector<u32> broadcastData;
  if (!linkWireless->getBroadcasts(broadcastData)) {
    log("Search failed :(");
    hang();
    return;
  }

  std::string str = "Press SELECT to connect\n";
  for (u32& number : broadcastData)
    str += std::to_string(number) + "\n";
  log(str);

  if (broadcastData.size() == 0) {
    log("Nothing found :(");
    hang();
    return;
  }

  waitFor(KEY_SELECT);

  if (!linkWireless->connect((u16)broadcastData[0])) {
    log("Connect failed :(");
    hang();
    return;
  }

  LinkWireless::ClientIdResponse response1;
  do {
    response1 = linkWireless->checkConnection();
    if (!response1.success) {
      log("Check connection failed :(");
      hang();
      return;
    }

    log("Checking: " + std::to_string(response1.clientId));
  } while (response1.clientId == 0);

  log("Assigned id (press SELECT):\n" + std::to_string(response1.clientId));

  waitFor(KEY_SELECT);

  auto response2 = linkWireless->finishConnection();
  if (!response2.success) {
    log("Finish connection failed :(");
    hang();
    return;
  }
  if (response2.clientId != response1.clientId) {
    log("Assigned IDs don't match :(");
    hang();
    return;
  }

  log("Connected! " + std::to_string(response2.clientId));

  messageLoop(false);
}

void messageLoop(bool acceptNewClients) {
  u32 i = 50;
  bool sending = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    if (!sending && (keys & KEY_A)) {
      sending = true;
      linkWireless->sendData(std::vector<u32>{1, i});
      i++;
    }
    if (sending && !(keys & KEY_A))
      sending = false;

    if (acceptNewClients) {
      auto newConnection = linkWireless->acceptConnection();
      if (!newConnection.success) {
        log("Accept failed :(");
        hang();
        return;
      }
      if (newConnection.clientIds.size() > 1)
        log("New connection: " + std::to_string(newConnection.clientIds[1]));
    }

    std::vector<u32> receivedData = std::vector<u32>{};
    if (!linkWireless->receiveData(receivedData)) {
      log("Receive failed :(");
      hang();
      return;
    }
    if (receivedData.size() > 1)
      log("<<< " + std::to_string(receivedData[1]));

    vBlankWait();
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

void vBlankWait() {
  while (REG_VCOUNT >= 160)
    ;  // wait till VDraw
  while (REG_VCOUNT < 160)
    ;  // wait till VBlank
}

void hang() {
  waitFor(KEY_DOWN);
}