#define LINK_ENABLE_DEBUG_LOGS 0

// (0) Include the header
#include "../../../lib/LinkMobile.hpp"

#include "main.h"

#include <tonc.h>
#include <functional>
#include "../../_lib/interrupt.h"

// One transfer for every N frames
constexpr static int TRANSFER_FREQUENCY = 30;

bool isConnected = false;
bool hasError = false;
u16 keys = 0;
std::string output = "";

LinkMobile::DataTransfer dataTransfer;
LinkMobile::DataTransfer lastCompletedTransfer;
LinkMobile::DNSQuery dnsQuery;
bool waitingDNS = false;
std::string outgoingData = "";
u32 counter = 0;
u32 frameCounter = 0;

bool left = false, right = false, up = false, down = false;
bool a = false, b = false, l = false, r = false;
bool start = false, select = false;
std::string selectedNumber = "";
std::string selectedPassword = "";
std::string selectedDomain = "";

LinkMobile* linkMobile = nullptr;

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

int main() {
  init();

start:
  log("LinkMobile_demo (v7.0.0)\n\nPress A to start");
  waitForA();

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

  while (true) {
    keys = ~REG_KEYS & KEY_ANY;
    hasError = linkMobile->getError().type != LinkMobile::Error::Type::NONE;
    output = "State = " + getStateString(linkMobile->getState()) + "\n";

    printMenu();

    if (linkMobile->isConnectedP2P()) {
      handleP2P();
    } else if (linkMobile->isConnectedPPP()) {
      handlePPP();
    } else if (isConnected) {
      cleanup();
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
        linkMobile = nullptr;

        if (!didShutdown) {
          log("Waiting...");
          wait(228 * 60 * 3);
        }

        goto start;
      } else if (linkMobile->canShutdown()) {
        // (12) Turn off the adapter
        linkMobile->shutdown();
      }
    }

    switch (linkMobile->getState()) {
      case LinkMobile::State::SESSION_ACTIVE: {
        // L = Read Configuration
        if (didPress(KEY_L, l)) {
          readConfiguration();
          waitForA();
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
          std::string password = getPasswordInput();
          if (password != "") {
            // (7) Connect to the internet
            linkMobile->callISP(password.c_str());
          }
        }
        break;
      }
      case LinkMobile::State::CALL_ESTABLISHED: {
        // L = hang up
        if (didPress(KEY_L, l)) {
          // (6) Hang up
          linkMobile->hangUp();
        }
        break;
      }
      case LinkMobile::State::PPP_ACTIVE: {
        // A = DNS query
        if (didPress(KEY_A, a) && !waitingDNS) {
          std::string domain = getDomainInput();
          if (domain != "") {
            // (8) Run DNS queries
            linkMobile->dnsQuery(domain.c_str(), &dnsQuery);
            waitingDNS = true;
          }
        }
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
  }

  return 0;
}

void handleP2P() {
  if (!isConnected) {
    // First transfer
    isConnected = true;
    outgoingData = linkMobile->getRole() == LinkMobile::Role::CALLER
                       ? "caller!!!"
                       : "receiver!!!";
    transfer(dataTransfer, outgoingData, 0xff, true);
  }

  if (dataTransfer.completed) {
    // Save a copy of last received data
    if (dataTransfer.size > 0)
      lastCompletedTransfer = dataTransfer;
    dataTransfer.completed = false;
  }

  if (keys & KEY_A) {
    // `A` increments the counter
    counter++;
    outgoingData =
        (linkMobile->getRole() == LinkMobile::Role::CALLER ? "caller: "
                                                           : "receiver: ") +
        std::to_string(counter);
  }

  frameCounter++;
  if (frameCounter >= TRANSFER_FREQUENCY) {
    // Transfer every N frames
    frameCounter = 0;
    transfer(dataTransfer, outgoingData, 0xff, true);
  }

  if (lastCompletedTransfer.completed) {
    // Show received data
    output += "\n\n>> " + std::string(outgoingData);
    output += "\n<< " + std::string((char*)lastCompletedTransfer.data);
    // (LinkMobile zero-pads an extra byte, so this is safe)
  }
}

void handlePPP() {
  if (!isConnected)
    isConnected = true;

  if (waitingDNS && dnsQuery.completed) {
    waitingDNS = false;
    log("DNS Response:\n  " + std::to_string(dnsQuery.ipv4[0]) + "." +
        std::to_string(dnsQuery.ipv4[1]) + "." +
        std::to_string(dnsQuery.ipv4[2]) + "." +
        std::to_string(dnsQuery.ipv4[3]) + "\n\n" +
        (dnsQuery.success ? "OK!\nLet's connect to it on TCP 80!"
                          : "DNS query failed!"));
    waitForA();
    if (!dnsQuery.success)
      return;

    // (9) Open connections
    log("Connecting...");
    LinkMobile::OpenConn openConn;
    linkMobile->openConnection(dnsQuery.ipv4, 80,
                               LinkMobile::ConnectionType::TCP, &openConn);
    if (!linkMobile->waitFor(&openConn)) {
      log("Connection failed!");
      waitForA();
      return;
    }

    // HTTP request

    LinkMobile::DataTransfer http;
    std::string request =
        std::string("GET / HTTP/1.1\r\nHost: ") + selectedDomain + "\r\n\r\n";
    std::string output = "";
    u32 chunk = 1;
    u32 retry = 1;
    do {
      log("Downloading... (" + std::to_string(chunk) + ", " +
          std::to_string(retry) + ")\n (hold START = close conn)\n\n" + output);

      if (didPress(KEY_START, start)) {
        log("Closing...");
        LinkMobile::CloseConn closeConn;
        linkMobile->closeConnection(
            openConn.connectionId, LinkMobile::ConnectionType::TCP, &closeConn);
        linkMobile->waitFor(&closeConn);
        return;
      }

      transfer(http, request, openConn.connectionId);
      if (!linkMobile->waitFor(&http)) {
        log("Connection closed:\n  " + std::to_string(chunk) + " packets!\n\n" +
            output);
        waitForA();
        return;
      }

      if (http.size > 0) {
        chunk++;
        output += std::string((char*)http.data);
        // (LinkMobile zero-pads an extra byte, so this is safe)
      }

      http = {};
      request = "";
      retry++;
    } while (true);
  }

  output += waitingDNS ? "\n\nWaiting DNS..." : "";
}

void cleanup() {
  isConnected = false;
  dataTransfer = {};
  lastCompletedTransfer = {};
  dnsQuery = {};
  waitingDNS = false;
  counter = 0;
  frameCounter = 0;
  outgoingData = "";
}

void readConfiguration() {
  LinkMobile::ConfigurationData data;
  if (!linkMobile->readConfiguration(data))
    log("Read failed :(");

  log("Magic:\n  " + toStr(data.magic, 2) + ", $" +
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

void printMenu() {
  auto error = linkMobile->getError();

  if (hasError) {
    output += getErrorString(error);
    output += "\n (SELECT = stop)";
  } else if (linkMobile->getState() == LinkMobile::State::SESSION_ACTIVE) {
    output += "\nL = Read configuration";
    output += "\nR = Call someone";
    output += "\nSTART = Call the ISP";
    output += "\n\n (A = ok)\n (SELECT = stop)";
  } else {
    if (linkMobile->isConnectedP2P()) {
      output += "\n (A = send)";
      output += "\n (L = hang up)";
    } else if (linkMobile->isConnectedPPP()) {
      output += "\n (A = DNS query)";
      output += "\n (L = hang up)";
    }
    output += "\n (SELECT = stop)";
  }
}

void transfer(LinkMobile::DataTransfer& dataTransfer,
              std::string text,
              u8 connectionId,
              bool addNullTerminator) {
  // (5) Send/receive data
  for (u32 i = 0; i < text.size(); i++)
    dataTransfer.data[i] = text[i];
  if (addNullTerminator)
    dataTransfer.data[text.size()] = '\0';
  dataTransfer.size = text.size() + addNullTerminator;
  linkMobile->transfer(dataTransfer, &dataTransfer, connectionId);
}

std::string getNumberInput() {
  std::vector<std::vector<std::string>> rows;
  rows.push_back({"1", "2", "3"});
  rows.push_back({"4", "5", "6"});
  rows.push_back({"7", "8", "9"});
  rows.push_back({"*", "0", "#"});
  std::vector<std::vector<std::string>> altRows;

  return getInput(selectedNumber, LINK_MOBILE_MAX_PHONE_NUMBER_LENGTH,
                  "a number", rows, altRows, {{"localhost", "127000000001"}},
                  "");
}

std::string getPasswordInput() {
  return getTextInput(selectedPassword, LINK_MOBILE_MAX_PASSWORD_LENGTH,
                      "your password", {{"pass123", "pass123"}});
}

std::string getDomainInput() {
  return getTextInput(
      selectedDomain, LINK_MOBILE_MAX_DOMAIN_NAME_LENGTH, "a domain name",
      {{"something.com", "something.com"}, {"localhost", "localhost"}});
}

std::string getTextInput(std::string& field,
                         u32 maxChars,
                         std::string inputName,
                         std::vector<DefaultValue> defaultValues) {
  std::vector<std::vector<std::string>> rows;
  rows.push_back({"a", "b", "c", "d", "e"});
  rows.push_back({"f", "g", "h", "i", "j"});
  rows.push_back({"k", "l", "m", "n", "o"});
  rows.push_back({"p", "q", "r", "s", "t"});
  rows.push_back({"u", "v", "w", "x", "y"});
  rows.push_back({"z", "1", "2", "3", "4"});
  rows.push_back({"5", "6", "7", "8", "9"});
  rows.push_back({"0", ".", "#", "/", "?"});

  std::vector<std::vector<std::string>> altRows;
  altRows.push_back({"A", "B", "C", "D", "E"});
  altRows.push_back({"F", "G", "H", "I", "J"});
  altRows.push_back({"K", "L", "M", "N", "O"});
  altRows.push_back({"P", "Q", "R", "S", "T"});
  altRows.push_back({"U", "V", "W", "X", "Y"});
  altRows.push_back({"Z", "1", "2", "3", "4"});
  altRows.push_back({"5", "6", "7", "8", "9"});
  altRows.push_back({"0", ".", "#", "/", "?"});

  return getInput(field, maxChars, inputName, rows, altRows, defaultValues,
                  "caps lock");
}

std::string getInput(std::string& field,
                     u32 maxChars,
                     std::string inputName,
                     std::vector<std::vector<std::string>> rows,
                     std::vector<std::vector<std::string>> altRows,
                     std::vector<DefaultValue> defaultValues,
                     std::string altName) {
  VBlankIntrWait();

  int selectedX = 0;
  int selectedY = 0;
  int selectedDefaultValue = 0;
  bool altActive = false;

  while (true) {
    auto renderRows = altActive ? altRows : rows;

    std::string output = "Type " + inputName + ":\n\n";
    output += ">> " + field + "\n\n";

    if (didPress(KEY_RIGHT, right)) {
      selectedX++;
      if (selectedX >= (int)renderRows[0].size())
        selectedX = renderRows[0].size() - 1;
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
      if (selectedY >= (int)renderRows.size())
        selectedY = renderRows.size() - 1;
    }
    if (didPress(KEY_B, b)) {
      if (field.size() > 0)
        field = field.substr(0, field.size() - 1);
      else
        return "";
    }
    if (didPress(KEY_A, a)) {
      if (field.size() < maxChars)
        field += renderRows[selectedY][selectedX];
    }
    if (didPress(KEY_SELECT, select)) {
      field = defaultValues[selectedDefaultValue].value;
      selectedDefaultValue = (selectedDefaultValue + 1) % defaultValues.size();
    }
    if (didPress(KEY_START, start))
      return field;
    if (altName != "" && didPress(KEY_L, l))
      altActive = !altActive;

    for (int y = 0; y < (int)renderRows.size(); y++) {
      for (int x = 0; x < (int)renderRows[y].size(); x++) {
        bool isSelected = selectedX == x && selectedY == y;
        output += std::string("") + "|" + (isSelected ? "<" : " ") +
                  renderRows[y][x] + (isSelected ? ">" : " ") + "|";
        output += " ";
      }
      output += "\n";
    }

    output += "\n (B = back)\n (A = select)\n (SELECT = " +
              defaultValues[selectedDefaultValue].name +
              ")\n (START = confirm)";

    if (altName != "")
      output += "\n\n (L = " + altName + ")";

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
    case LinkMobile::State::PPP_LOGIN:
      return "PPP_LOGIN";
    case LinkMobile::State::PPP_ACTIVE:
      return "PPP_ACTIVE";
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
         "\n  ReqType: " + std::to_string(error.reqType) + "\n";
}

std::string getErrorTypeString(LinkMobile::Error::Type errorType) {
  switch (errorType) {
    case LinkMobile::Error::Type::ADAPTER_NOT_CONNECTED:
      return "ADAPTER_NOT_CONNECTED";
    case LinkMobile::Error::Type::PPP_LOGIN_FAILED:
      return "PPP_LOGIN_FAILED";
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

void log(std::string text) {
  if (linkMobile != nullptr)
    VBlankIntrWait();
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
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
