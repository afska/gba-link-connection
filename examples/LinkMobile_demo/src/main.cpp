#define LINK_ENABLE_DEBUG_LOGS 1

// (0) Include the header
#include "../../../lib/LinkMobile.hpp"

#include <tonc.h>
#include <functional>
#include <string>
#include <vector>
#include "../../_lib/interrupt.h"

std::string readConfiguration();
std::string getStateString(LinkMobile::State state);
std::string getErrorString(LinkMobile::Error error);
std::string getErrorTypeString(LinkMobile::Error::Type errorType);
std::string getResultString(LinkMobile::CommandResult cmdResult);
void log(std::string text);
std::string toStr(char* chars, int size);
void waitFor(u16 key);
void wait(u32 verticalLines);
void hang();

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len = sizeof(I) << 1);

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
  u16 initialKeys = ~REG_KEYS & KEY_ANY;
  bool dontReceiveCalls = (initialKeys & KEY_START);  // TODO: REMOVE

  // (1) Create a LinkMobile instance
  linkMobile = new LinkMobile(dontReceiveCalls);

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
  bool calling = false;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // Menu
    std::string output = "";
    bool shouldHang = false;
    output += "State = " + getStateString(linkMobile->getState()) + "\n\n";

    auto error = linkMobile->getError();
    bool hasError = error.type != LinkMobile::Error::NONE;
    if (hasError) {
      output += getErrorString(error);
      output += " (SELECT = stop)";
    } else if (linkMobile->getState() == LinkMobile::State::SESSION_ACTIVE) {
      output += "L = Read Configuration\n";
      output += "R = Call localhost\n\n";
      output += " (DOWN = ok)\n (SELECT = stop)";
    } else {
      output += " (SELECT = stop)";
    }

    // SELECT = stop
    if (keys & KEY_SELECT) {
      bool didShutdown = linkMobile->getState() == LinkMobile::State::SHUTDOWN;
      if (hasError || didShutdown) {
        linkMobile->deactivate();
        interrupt_disable(INTR_VBLANK);
        interrupt_disable(INTR_SERIAL);
        interrupt_disable(INTR_TIMER3);
        delete linkMobile;
        linkMobile = NULL;

        if (!didShutdown) {
          log("Waiting...");
          wait(228 * 60 * 3);
        }

        goto start;
      } else if (linkMobile->canShutdown()) {
        linkMobile->shutdown();
      }
    }

    // L = Read Configuration
    if ((keys & KEY_L) && !reading) {
      reading = true;
      output = readConfiguration();
      shouldHang = true;
    }
    if (reading && !(keys & KEY_L))
      reading = false;

    // R = Call localhost
    if ((keys & KEY_R) && !calling) {
      calling = true;
      linkMobile->call("127000000001");
    }
    if (calling && !(keys & KEY_R))
      calling = false;

    VBlankIntrWait();
    log(output);
    if (shouldHang)
      hang();
  }

  return 0;
}

std::string readConfiguration() {
  LinkMobile::ConfigurationData data;
  if (!linkMobile->readConfiguration(data))
    return "Read failed :(";

  return ("Magic:\n  " + toStr(data.magic, 2) + "\nIsRegistering:\n  " +
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
          "\nSMTP Server:\n  " + toStr(data.smtpServer, 20) +
          "\nPOP Server:\n  " + toStr(data.popServer, 19) + "\n\nMode: " +
          (linkMobile->getDataSize() == LinkSPI::DataSize::SIZE_32BIT
               ? "SIO32"
               : "SIO8"));
}

std::string getStateString(LinkMobile::State state) {
  switch (state) {
    case LinkMobile::State::NEEDS_RESET:
      return "NEEDS_RESET";
    case LinkMobile::State::PINGING:
      return "PINGING";
    case LinkMobile::State::WAITING_TO_START:
      return "WAITING_TO_START";
    case LinkMobile::State::STARTING_SESSION:
      return "STARTING_SESSION";
    case LinkMobile::State::ACTIVATING_SIO32:
      return "ACTIVATING_SIO32";
    case LinkMobile::State::WAITING_32BIT_SWITCH:
      return "WAITING_32BIT_SWITCH";
    case LinkMobile::State::READING_CONFIGURATION:
      return "READING_CONFIGURATION";
    case LinkMobile::State::SESSION_ACTIVE:
      return "SESSION_ACTIVE";
    case LinkMobile::State::CALL_REQUESTED:
      return "CALL_REQUESTED";
    case LinkMobile::State::CALLING:
      return "CALLING";
    case LinkMobile::State::CALL_ESTABLISHED:
      return "CALL_ESTABLISHED";
    case LinkMobile::State::SHUTDOWN_REQUESTED:
      return "SHUTDOWN_REQUESTED";
    case LinkMobile::State::ENDING_SESSION:
      return "ENDING_SESSION";
    case LinkMobile::State::WAITING_8BIT_SWITCH:
      return "WAITING_8BIT_SWITCH";
    case LinkMobile::State::SHUTDOWN:
      return "SHUTDOWN";
    default:
      return "?";
  }
}

std::string getErrorString(LinkMobile::Error error) {
  return "ERROR"
         "\n  Type: " +
         getErrorTypeString(error.type) +
         "\n  State: " + getStateString(error.state) +
         "\n  CmdID: " + std::string(error.cmdIsSending ? ">" : "<") + "$" +
         toHex(error.cmdId) +
         "\n  CmdResult: " + getResultString(error.cmdResult) +
         "\n  CmdErrorCode: " + std::to_string(error.cmdErrorCode) + "\n\n";
}

std::string getErrorTypeString(LinkMobile::Error::Type errorType) {
  switch (errorType) {
    case LinkMobile::Error::Type::ADAPTER_NOT_CONNECTED:
      return "ADAPTER_NOT_CONNECTED";
    case LinkMobile::Error::Type::COMMAND_FAILED:
      return "COMMAND_FAILED";
    case LinkMobile::Error::Type::WEIRD_RESPONSE:
      return "WEIRD_RESPONSE";
    case LinkMobile::Error::Type::BAD_CONFIGURATION_CHECKSUM:
      return "BAD_CONFIGURATION_CHECKSUM";
    default:
      return "?";
  }
}

std::string getResultString(LinkMobile::CommandResult cmdResult) {
  switch (cmdResult) {
    case LinkMobile::CommandResult::PENDING:
      return "PENDING";
    case LinkMobile::CommandResult::SUCCESS:
      return "SUCCESS";
    case LinkMobile::CommandResult::NOT_WAITING:
      return "NOT_WAITING";
    case LinkMobile::CommandResult::INVALID_DEVICE_ID:
      return "INVALID_DEVICE_ID";
    case LinkMobile::CommandResult::INVALID_COMMAND_ACK:
      return "INVALID_COMMAND_ACK";
    case LinkMobile::CommandResult::INVALID_MAGIC_BYTES:
      return "INVALID_MAGIC_BYTES";
    case LinkMobile::CommandResult::WEIRD_DATA_SIZE:
      return "WEIRD_DATA_SIZE";
    case LinkMobile::CommandResult::WRONG_CHECKSUM:
      return "WRONG_CHECKSUM";
    case LinkMobile::CommandResult::ERROR_CODE:
      return "ERROR_CODE";
    case LinkMobile::CommandResult::WEIRD_ERROR_CODE:
      return "WEIRD_ERROR_CODE";
    case LinkMobile::CommandResult::TIMEOUT:
      return "TIMEOUT";
    default:
      return "?";
  }
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

std::string toStr(char* chars, int size) {
  char copiedChars[255];
  for (int i = 0; i < size; i++)
    copiedChars[i] = chars[i];
  copiedChars[size] = '\0';
  return std::string(copiedChars);
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

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len) {
  static const char* digits = "0123456789ABCDEF";
  std::string rc(hex_len, '0');
  for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
    rc[i] = digits[(w >> j) & 0x0f];
  return rc;
}
