// (0) Include the header
#include "../../../lib/LinkMobile.hpp"

#include <tonc.h>
#include <functional>
#include <string>
#include <vector>
#include "../../_lib/interrupt.h"

#define CHECK_ERRORS(MESSAGE)                                             \
  if ((lastError = linkMobile->getLastError()) ||                         \
      linkMobile->getState() == LinkMobile::State::NEEDS_RESET) {         \
    log(std::string(MESSAGE) + " (" + std::to_string(lastError) + ") [" + \
        std::to_string(linkMobile->getState()) + "]");                    \
    hang();                                                               \
    linkMobile->activate();                                               \
    return;                                                               \
  }

void activate();
void readConfiguration();
void log(std::string text);
void waitFor(u16 key);
void wait(u32 verticalLines);
void hang();

LinkMobile::Error lastError;
LinkMobile* linkMobile = NULL;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

  bool firstTime = true;

start:
  // Options
  log("LinkMobile_demo (v6.7.0)\n\n\n\n"
      "Press A to start");
  waitFor(KEY_A);
  // u16 initialKeys = ~REG_KEYS & KEY_ANY;
  // forwarding = !(initialKeys & KEY_LEFT);
  // retransmission = !(initialKeys & KEY_UP);
  // maxPlayers = (initialKeys & KEY_B) ? 2 : LINK_MOBILE_MAX_PLAYERS;
  // asyncACK = initialKeys & KEY_START;

  // (1) Create a LinkMobile instance
  linkMobile = new LinkMobile();
  linkMobile->debug = [](std::string str) { log(str); };

  if (firstTime) {
    // (2) Add the required interrupt service routines
    interrupt_init();
    interrupt_set_handler(INTR_VBLANK, LINK_MOBILE_ISR_VBLANK);
    interrupt_enable(INTR_VBLANK);
    interrupt_set_handler(INTR_SERIAL, LINK_MOBILE_ISR_SERIAL);
    interrupt_enable(INTR_SERIAL);
    interrupt_set_handler(INTR_TIMER3, LINK_MOBILE_ISR_TIMER);
    interrupt_enable(INTR_TIMER3);

    firstTime = false;
  }

again:
  // (3) Initialize the library
  if (!linkMobile->activate()) {
    log("Adapter not connected!\n\nPress A to try again");
    waitFor(KEY_A);
    goto again;
  }

  bool activating = false;
  bool reading = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // Menu
    log(std::string("") +
        "L = Read Configuration\n\n "
        "(SELECT = cancel)\n (START = reactivate)");

    // SELECT = back
    if (keys & KEY_SELECT) {
      linkMobile->deactivate();
      delete linkMobile;
      linkMobile = NULL;
      goto start;
    }

    // START = Activate
    if ((keys & KEY_START) && !activating) {
      activating = true;
      activate();
    }
    if (activating && !(keys & KEY_START))
      activating = false;

    // L = Read Configuration
    if ((keys & KEY_L) && !reading) {
      reading = true;
      readConfiguration();
    }
    if (reading && !(keys & KEY_L))
      reading = false;

    VBlankIntrWait();
  }

  return 0;
}

void activate() {
  log("Trying...");

  if (linkMobile->activate())
    log("Activated!");
  else
    log("Activation failed! :(");

  hang();
}

std::string toStr(char* chars, int size) {
  char copiedChars[255];
  for (int i = 0; i < size; i++)
    copiedChars[i] = chars[i];
  copiedChars[size] = '\0';
  return std::string(copiedChars);
}

void readConfiguration() {
  log("Reading...");

  LinkMobile::ConfigurationData data;
  if (!linkMobile->readConfiguration(data)) {
    log("Read failed :(");
    hang();
    return;
  }

  log("Magic:\n  " + toStr(data.magic, 2) + "\nIsRegistering:\n  " +
      (data.isRegistering ? "Yes" : "No") + "\nPrimary DNS:\n  " +
      std::to_string(data.primaryDNS[0]) + "." +
      std::to_string(data.primaryDNS[1]) + "." +
      std::to_string(data.primaryDNS[2]) + "." +
      std::to_string(data.primaryDNS[3]) + "\nSecondary DNS:\n  " +
      std::to_string(data.secondaryDNS[0]) + "." +
      std::to_string(data.secondaryDNS[1]) + "." +
      std::to_string(data.secondaryDNS[2]) + "." +
      std::to_string(data.secondaryDNS[3]) + "\nLoginID:\n  " +
      toStr(data.loginID, 10) + "\nEmail:\n  " + toStr(data.email, 24) +
      "\nSMTP Server:\n  " + toStr(data.smtpServer, 20) + "\nPOP Server:\n  " +
      toStr(data.popServer, 19));
  hang();
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

void hang() {
  waitFor(KEY_DOWN);
}
