#define LINK_ENABLE_DEBUG_LOGS 1

// (0) Include the header
#include "../../../lib/LinkMobile.hpp"

#include <tonc.h>
#include <functional>
#include <string>
#include <vector>
#include "../../_lib/interrupt.h"

void transfer(LinkMobile::DataTransfer& dataTransfer, std::string text);
std::string readConfiguration();
std::string getNumberInput();
std::string getStateString(LinkMobile::State state);
std::string getErrorString(LinkMobile::Error error);
std::string getErrorTypeString(LinkMobile::Error::Type errorType);
std::string getResultString(LinkMobile::CommandResult cmdResult);
void log(std::string text);
std::string toStr(char* chars, int size);
void wait(u32 verticalLines);
bool didPress(u16 key, bool& pressed);
void waitForA();

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len = sizeof(I) << 1);
bool left = false, right = false, up = false, down = false;
bool a = false, b = false, l = false, r = false;
bool start = false, select = false;
std::string selectedNumber = "";

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
  waitForA();

  // (1) Create a LinkWireless instance
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

  bool isConnected = false;
  LinkMobile::DataTransfer dataTransfer;
  LinkMobile::DataTransfer lastCompletedTransfer;
  std::string outgoingData = "";
  u32 counter = 0;
  u32 frameCounter = 0;

  while (true) {
    // (one transfer for every N frames)
    constexpr static int TRANSFER_FREQUENCY = 30;

    u16 keys = ~REG_KEYS & KEY_ANY;

    // Menu
    std::string output = "";
    bool shouldWaitForA = false;
    output += "State = " + getStateString(linkMobile->getState()) + "\n";

    auto error = linkMobile->getError();
    bool hasError = error.type != LinkMobile::Error::NONE;
    if (hasError) {
      output += getErrorString(error);
      output += "\n (SELECT = stop)";
    } else if (linkMobile->getState() == LinkMobile::State::SESSION_ACTIVE) {
      output += "\nL = Read Configuration";
      output += "\nR = Call someone";
      output += "\nSTART = Call the ISP";
      output += "\n (A = ok)\n (SELECT = stop)";
    } else {
      if (linkMobile->isConnectedP2P()) {
        output += "\n (A = send)";
        output += "\n (L = hang up)";
      } else if (linkMobile->isConnectedISP()) {
        output += "\n (L = hang up)";
      }
      output += "\n (SELECT = stop)";
    }

    if (linkMobile->isConnectedP2P()) {
      if (!isConnected) {
        isConnected = true;
        outgoingData = linkMobile->getRole() == LinkMobile::Role::CALLER
                           ? "caller!!!"
                           : "receiver!!!";
        transfer(dataTransfer, outgoingData);
      }

      if (dataTransfer.completed) {
        if (dataTransfer.size > 0)
          lastCompletedTransfer = dataTransfer;

        if (keys & KEY_A) {
          counter++;
          outgoingData = (linkMobile->getRole() == LinkMobile::Role::CALLER
                              ? "caller: "
                              : "receiver: ") +
                         std::to_string(counter);
        }

        frameCounter++;
        if (frameCounter >= TRANSFER_FREQUENCY) {
          frameCounter = 0;
          transfer(dataTransfer, outgoingData);
        }
      }
      if (lastCompletedTransfer.completed) {
        char received[LINK_MOBILE_MAX_USER_TRANSFER_LENGTH];
        for (u32 i = 0; i < lastCompletedTransfer.size; i++)
          received[i] = lastCompletedTransfer.data[i];
        received[lastCompletedTransfer.size] = '\0';
        output += "\n\n>> " + std::string(outgoingData);
        output += "\n<< " + std::string(received);
      }
    } else {
      if (isConnected) {
        isConnected = false;
        dataTransfer = {};
        lastCompletedTransfer = {};
        counter = 0;
        frameCounter = 0;
        outgoingData = "";
      }
    }

    // SELECT = stop
    if (didPress(KEY_SELECT, select)) {
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
        // (7) Turn off the adapter
        linkMobile->shutdown();
      }
    }

    switch (linkMobile->getState()) {
      case LinkMobile::State::SESSION_ACTIVE: {
        // L = Read Configuration
        if (didPress(KEY_L, l)) {
          output = readConfiguration();
          shouldWaitForA = true;
        }

        // R = Call someone
        if (didPress(KEY_R, r)) {
          std::string number = getNumberInput();
          if (number != "") {
            // (4) Call someone
            linkMobile->call(number.c_str());
          }
        }

        // START = Call the ISP
        if (didPress(KEY_START, start)) {
          linkMobile->callISP("asdasd");
        }
        break;
      }
      case LinkMobile::State::CALL_ESTABLISHED:
      case LinkMobile::State::ISP_ACTIVE: {
        // L = hang up
        if (didPress(KEY_L, l)) {
          // (6) Hang up
          linkMobile->hangUp();
        }
        break;
      }
      default: {
      }
    }

    VBlankIntrWait();
    log(output);
    if (shouldWaitForA)
      waitForA();
  }

  return 0;
}

void transfer(LinkMobile::DataTransfer& dataTransfer, std::string text) {
  // (5) Send/receive data
  for (u32 i = 0; i < text.size(); i++)
    dataTransfer.data[i] = text[i];
  dataTransfer.data[text.size()] = '\0';
  dataTransfer.size = text.size() + 1;
  linkMobile->transfer(dataTransfer, &dataTransfer);
}

std::string readConfiguration() {
  LinkMobile::ConfigurationData data;
  if (!linkMobile->readConfiguration(data))
    return "Read failed :(";

  return (
      "Magic:\n  " + toStr(data.magic, 2) + ", $" +
      toHex(data.registrationState) + "\nPrimary DNS:\n  " +
      std::to_string(data.primaryDNS[0]) + "." +
      std::to_string(data.primaryDNS[1]) + "." +
      std::to_string(data.primaryDNS[2]) + "." +
      std::to_string(data.primaryDNS[3]) + "\nSecondary DNS:\n  " +
      std::to_string(data.secondaryDNS[0]) + "." +
      std::to_string(data.secondaryDNS[1]) + "." +
      std::to_string(data.secondaryDNS[2]) + "." +
      std::to_string(data.secondaryDNS[3]) + "\nLoginID:\n  " +
      toStr(data.loginId, 10) + "\nEmail:\n  " + toStr(data.email, 24) +
      "\nSMTP Server:\n  " + toStr(data.smtpServer, 20) + "\nPOP Server:\n  " +
      toStr(data.popServer, 19) + "\nISP Number #1:\n  " +
      std::string(data._ispNumber1) + "\n\nIs Valid: " +
      std::to_string(linkMobile->isConfigurationValid()) + "\nMode: " +
      (linkMobile->getDataSize() == LinkSPI::DataSize::SIZE_32BIT ? "SIO32"
                                                                  : "SIO8"));
}

std::string getNumberInput() {
  VBlankIntrWait();

  int selectedX = 0;
  int selectedY = 0;
  std::vector<std::vector<std::string>> rows;
  rows.push_back({"1", "2", "3"});
  rows.push_back({"4", "5", "6"});
  rows.push_back({"7", "8", "9"});
  rows.push_back({"*", "0", "#"});

  while (true) {
    std::string output = "Type a number:\n\n";
    output += ">> " + selectedNumber + "\n\n";

    if (didPress(KEY_RIGHT, right)) {
      selectedX++;
      if (selectedX >= (int)rows[0].size())
        selectedX = rows[0].size() - 1;
    }
    if (didPress(KEY_LEFT, left)) {
      selectedX--;
      if (selectedX < 0)
        selectedX = 0;
    }
    if (didPress(KEY_UP, up)) {
      selectedY--;
      if (selectedY < 0)
        selectedY = 0;
    }
    if (didPress(KEY_DOWN, down)) {
      selectedY++;
      if (selectedY >= (int)rows.size())
        selectedY = rows.size() - 1;
    }
    if (didPress(KEY_B, b)) {
      if (selectedNumber.size() > 0)
        selectedNumber = selectedNumber.substr(0, selectedNumber.size() - 1);
      else
        return "";
    }
    if (didPress(KEY_A, a)) {
      if (selectedNumber.size() < LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH)
        selectedNumber += rows[selectedY][selectedX];
    }
    if (didPress(KEY_SELECT, select))
      selectedNumber = "127000000001";
    if (didPress(KEY_START, start))
      return selectedNumber;

    for (int y = 0; y < (int)rows.size(); y++) {
      for (int x = 0; x < (int)rows[y].size(); x++) {
        bool isSelected = selectedX == x && selectedY == y;
        output += std::string("") + "|" + (isSelected ? "<" : " ") +
                  rows[y][x] + (isSelected ? ">" : " ") + "|";
        output += " ";
      }
      output += "\n";
    }

    output +=
        "\n (B = back)\n (A = select)\n (SELECT = localhost)\n (START = "
        "confirm)";

    VBlankIntrWait();
    log(output);
  }
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
    case LinkMobile::State::ISP_CALL_REQUESTED:
      return "ISP_CALL_REQUESTED";
    case LinkMobile::State::ISP_CALLING:
      return "ISP_CALLING";
    case LinkMobile::State::ISP_LOGIN:
      return "ISP_LOGIN";
    case LinkMobile::State::ISP_ACTIVE:
      return "ISP_ACTIVE";
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
         "\n  CmdErrorCode: " + std::to_string(error.cmdErrorCode) +
         "\n  ReqType: " + std::to_string(error.reqType) + "\n\n";
}

std::string getErrorTypeString(LinkMobile::Error::Type errorType) {
  switch (errorType) {
    case LinkMobile::Error::Type::ADAPTER_NOT_CONNECTED:
      return "ADAPTER_NOT_CONNECTED";
    case LinkMobile::Error::Type::ISP_LOGIN_FAILED:
      return "ISP_LOGIN_FAILED";
    case LinkMobile::Error::Type::COMMAND_FAILED:
      return "COMMAND_FAILED";
    case LinkMobile::Error::Type::WEIRD_RESPONSE:
      return "WEIRD_RESPONSE";
    case LinkMobile::Error::Type::TIMEOUT:
      return "TIMEOUT";
    case LinkMobile::Error::Type::WTF:
      return "WTF";
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

void waitForA() {
  while (!didPress(KEY_A, a))
    ;
}

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len) {
  static const char* digits = "0123456789ABCDEF";
  std::string rc(hex_len, '0');
  for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
    rc[i] = digits[(w >> j) & 0x0f];
  return rc;
}
