#define LINK_ENABLE_DEBUG_LOGS 1

// (0) Include the header
#include "../../../lib/LinkMobile.hpp"

#include <tonc.h>
#include <functional>
#include <string>
#include <vector>
#include "../../_lib/interrupt.h"

void activate();
void readConfiguration();
void log(std::string text);
void waitFor(u16 key);
void wait(u32 verticalLines);
void hang();

LinkMobile* linkMobile = NULL;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

start:
  // Options
  log("LinkMobile_demo (v7.0.0)\n\n"
      "Press A to start");
  waitFor(KEY_A);

  // (1) Create a LinkMobile instance
  linkMobile = new LinkMobile();

  // (2) Add the required interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, LINK_MOBILE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_MOBILE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_MOBILE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // (3) Initialize the library
  linkMobile->activate();

  bool reading = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // Menu
    log(std::string("") + "State = " + std::to_string(linkMobile->getState()) +
        "\n\n" +
        "L = Read Configuration\n\n (DOWN = ok)\n "
        "(SELECT = stop)");

    // SELECT = back
    if (keys & KEY_SELECT) {
      linkMobile->deactivate();
      interrupt_disable(INTR_VBLANK);
      interrupt_disable(INTR_SERIAL);
      interrupt_disable(INTR_TIMER3);
      delete linkMobile;
      linkMobile = NULL;
      goto start;
    }

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
  linkMobile->activate();
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

  while (linkMobile->getState() != LinkMobile::State::SESSION_ACTIVE)
    ;

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
      toStr(data.popServer, 19) + "\n\nMode: " +
      (linkMobile->getDataSize() == LinkSPI::DataSize::SIZE_32BIT ? "SIO32"
                                                                  : "SIO8"));
  while (true)
    ;
}

std::string lastLoggedText = "";
void log(std::string text) {
  if (text == lastLoggedText)
    return;
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
  lastLoggedText = text;
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
