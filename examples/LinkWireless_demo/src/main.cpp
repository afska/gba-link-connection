#include <tonc.h>
#include <string>

// (0) Include the header
#include "../../_lib/LinkWireless.h"

void log(std::string text);

// (1) Create a LinkWireless instance
LinkWireless* linkWireless = new LinkWireless();

void init() {
  linkWireless->debug = [](std::string text) { log(text); };  // TODO: REMOVE

  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Initialize the library
  // linkWireless->activate(); // TODO: RECOVER
}

int main() {
  init();

  bool activating = false;
  bool hosting = false;
  bool connecting = false;

  while (true) {
    // std::string output = "";
    u16 keys = ~REG_KEYS & KEY_ANY;

    if ((keys & KEY_START) && !activating) {
      log("Trying...");
      activating = true;
      if (linkWireless->activate())
        log("Activated! :)");
      else
        log("Activation failed! :(");
    }

    if (activating && !(keys & KEY_START))
      activating = false;

    if ((keys & KEY_L) && !hosting) {
      log("Hosting...");
      hosting = true;
      if (linkWireless->host(std::vector<u32>{0x0c020002, 0x00005ce1,
                                              0x00000000, 0x09000040,
                                              0xc1cfc8cd, 0x00ffccbb})) {
        log("Hosting ok. Listening...");
        u16 newId = 0;
        do {
          newId = linkWireless->getNewConnectionId();
          log("Hosting ok. " + std::to_string(newId) + " Listening... ");
        } while (newId == 0 || newId == 1);
        log("CONNECTED! " + std::to_string(newId));

        u32 i = 50;
        bool sending = false;

        // TODO: DEDUP
        while (true) {
          keys = ~REG_KEYS & KEY_ANY;

          if (!sending && (keys & KEY_A)) {
            sending = true;
            linkWireless->sendData(std::vector<u32>{i});
            i++;
          }

          if (sending && !(keys & KEY_A))
            sending = false;

          std::vector<u32> receivedData = std::vector<u32>{};
          if (!linkWireless->receiveData(receivedData)) {
            log("ERROR RECEIVING!");
            while (true)
              ;
          }
          if (receivedData.size() > 0)
            log("RECEIVED: " + std::to_string(receivedData[0]));

          while (REG_VCOUNT >= 160)
            ;  // wait till VDraw
          while (REG_VCOUNT < 160)
            ;  // wait till VBlank
        }
      } else {
        log("Hosting error");
        while (true)
          ;
      }
    }

    if (hosting && !(keys & KEY_L))
      hosting = false;

    if (!connecting && (keys & KEY_R)) {
      log("Searching...");
      connecting = true;
      std::vector<u32> data;
      if (linkWireless->getBroadcasts(data)) {
        std::string str =
            "Press select to conn " + std::to_string(data.size()) + "\n";
        for (u32& dat : data)
          str += std::to_string(dat) + "\n";
        log(str);

        if (data.size() > 0) {
          do {
            keys = ~REG_KEYS & KEY_ANY;
          } while (!(keys & KEY_SELECT));

          if (!linkWireless->connect((u16)data[0])) {
            log("CONNECT FAILED!");
            while (true)
              ;
          }

          u16 idid = 0;
          do {
            idid = linkWireless->isFinishedConnect();
          } while (idid <= 1);

          log("HAVE ID! PRESS SEL! " + std::to_string(idid));

          do {
            keys = ~REG_KEYS & KEY_ANY;
          } while (!(keys & KEY_SELECT));

          u16 asd = linkWireless->finishConnection();
          if (asd == 0) {
            log("FINISH CONNECT FAILED!");
            while (true)
              ;
          }

          u32 i = 50;
          bool sending = false;
          log("CONNECTED! " + std::to_string(asd));
          // TODO: DEDUP
          while (true) {
            keys = ~REG_KEYS & KEY_ANY;

            if (!sending && (keys & KEY_A)) {
              sending = true;
              linkWireless->sendData(std::vector<u32>{i});
              i++;
            }

            if (sending && !(keys & KEY_A))
              sending = false;

            std::vector<u32> receivedData = std::vector<u32>{};
            if (!linkWireless->receiveData(receivedData)) {
              log("ERROR RECEIVING!");
              while (true)
                ;
            }
            if (receivedData.size() > 0)
              log("RECEIVED: " + std::to_string(receivedData[0]));

            while (REG_VCOUNT >= 160)
              ;  // wait till VDraw
            while (REG_VCOUNT < 160)
              ;  // wait till VBlank
          }
        }
      } else
        log("Search failed :(");
    }

    if (connecting && !(keys & KEY_R))
      connecting = false;

    while (REG_VCOUNT >= 160)
      ;  // wait till VDraw
    while (REG_VCOUNT < 160)
      ;  // wait till VBlank
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}