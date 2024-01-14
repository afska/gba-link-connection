#include "DebugScene.h"

#include <libgba-sprite-engine/background/text_stream.h>
#include <tonc.h>
#include <functional>

#include "../../../../lib/LinkRawWireless.hpp"
#include "utils/InputHandler.h"
#include "utils/SceneUtils.h"

DebugScene::DebugScene(std::shared_ptr<GBAEngine> engine) : Scene(engine) {}

static std::unique_ptr<InputHandler> aHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> bHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> upHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> downHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> lHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> rHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> selectHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> startHandler =
    std::unique_ptr<InputHandler>(new InputHandler());

static std::vector<std::string> logLines;
static u32 currentLogLine = 0;
static bool useVerboseLog = true;

static const std::string CHARACTERS[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c",
    "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C",
    "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
    "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
static const u32 CHARACTERS_LEN = 62;

#define MAX_LINES 18
#define DRAW_LINE 2

void printScrollableText(u32 currentLine,
                         std::vector<std::string> lines,
                         bool withCursor = false) {
  for (u32 i = 0; i < MAX_LINES; i++) {
    u32 lastLineIndex = MAX_LINES - 1;
    u32 index = max(currentLine, lastLineIndex) - lastLineIndex + i;
    if (index < lines.size()) {
      std::string cursor = currentLine == index ? "> " : "  ";
      TextStream::instance().setText((withCursor ? cursor : "") + lines[index],
                                     DRAW_LINE + i, -3);
    } else {
      TextStream::instance().setText("                              ",
                                     DRAW_LINE + i, -3);
    }
  }
}

void print() {
  printScrollableText(currentLogLine, logLines);
}

void scrollBack() {
  if (currentLogLine <= 0)
    return;
  currentLogLine--;
  print();
}

void scrollForward() {
  if (currentLogLine < MAX_LINES - 1)
    currentLogLine = min(MAX_LINES - 1, logLines.size() - 1);
  if (currentLogLine == logLines.size() - 1)
    return;
  currentLogLine++;
  print();
}

void scrollPageUp() {
  currentLogLine = max(currentLogLine - MAX_LINES, 0);
  print();
}

void scrollPageDown() {
  currentLogLine = min(currentLogLine + MAX_LINES, logLines.size() - 1);
  print();
}

void scrollToTop() {
  currentLogLine = 0;
  print();
}

void scrollToBottom() {
  currentLogLine = logLines.size() - 1;
  print();
}

void clear() {
  logLines.clear();
  currentLogLine = 0;
  print();
}

void log(std::string string) {
  logLines.push_back(string);
  scrollPageDown();
}

std::vector<Background*> DebugScene::backgrounds() {
  return {};
}

std::vector<Sprite*> DebugScene::sprites() {
  std::vector<Sprite*> sprites;
  return sprites;
}

void DebugScene::load() {
  SCENE_init();
  BACKGROUND_enable(true, false, false, false);

  linkRawWireless->logger = [](std::string string) {
    if (useVerboseLog)
      log(string);
  };

  log("---");
  log("LinkRawWireless demo");
  log("");
  log("START: reset wireless adapter");
  log("A: send command");
  log("B: toggle log level");
  log("UP/DOWN: scroll up/down");
  log("L/R: scroll page up/down");
  log("UP+L/DOWN+R: scroll to top/bottom");
  log("SELECT: clear");
  log("---");
  log("");
  toggleLogLevel();

  addCommandMenuOptions();
}

void DebugScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  TextStream::instance().setText("state = AUTHENTICATED     p?/?", 0, -3);

  processKeys(keys);
  processButtons();

  __qran_seed += keys;
  __qran_seed += REG_RCNT;
  __qran_seed += REG_SIOCNT;
}

void DebugScene::addCommandMenuOptions() {
  commandMenuOptions.push_back("Setup+Broadcast+StartHost");
  commandMenuOptions.push_back("Setup+BroadcastRead1+2+3");
  commandMenuOptions.push_back("Setup+Connect+FinishConn");

  commandMenuOptions.push_back("0x10 (Hello)");
  commandMenuOptions.push_back("0x11 (SignalLevel)");
  commandMenuOptions.push_back("0x12 (VersionStatus)");
  commandMenuOptions.push_back("0x13 (SystemStatus)");
  commandMenuOptions.push_back("0x14 (SlotStatus)");
  commandMenuOptions.push_back("0x15 (ConfigStatus)");
  commandMenuOptions.push_back("0x16 (Broadcast)");
  commandMenuOptions.push_back("0x17 (Setup)");
  commandMenuOptions.push_back("0x18 (?)");
  commandMenuOptions.push_back("0x19 (StartHost)");
  commandMenuOptions.push_back("0x1a (AcceptConnections)");
  commandMenuOptions.push_back("0x1b (EndHost)");
  commandMenuOptions.push_back("0x1c (BroadcastRead1)");
  commandMenuOptions.push_back("0x1d (BroadcastRead2)");
  commandMenuOptions.push_back("0x1e (BroadcastRead3)");
  commandMenuOptions.push_back("0x1f (Connect)");
  commandMenuOptions.push_back("0x20 (IsFinishedConnect)");
  commandMenuOptions.push_back("0x21 (FinishConnection)");
  commandMenuOptions.push_back("0x24 (SendData)");
  commandMenuOptions.push_back("0x25 (SendDataAndWait)");
  commandMenuOptions.push_back("0x26 (ReceiveData)");
  commandMenuOptions.push_back("0x27 (Wait)");
  commandMenuOptions.push_back("0x30 (DisconnectClient)");
  commandMenuOptions.push_back("0x32 (?)");
  commandMenuOptions.push_back("0x33 (?)");
  commandMenuOptions.push_back("0x34 (?)");
  commandMenuOptions.push_back("0x35 (?!)");
  commandMenuOptions.push_back("0x37 (RetransmitAndWait)");
  commandMenuOptions.push_back("0x38 (?)");
  commandMenuOptions.push_back("0x39 (?)");
  commandMenuOptions.push_back("0x3d (Bye)");
}

void DebugScene::processKeys(u16 keys) {
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  upHandler->setIsPressed(keys & KEY_UP);
  downHandler->setIsPressed(keys & KEY_DOWN);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);
  startHandler->setIsPressed(keys & KEY_START);
}

void DebugScene::processButtons() {
  if (aHandler->hasBeenPressedNow()) {
    int selectedCommandIndex =
        selectOption("Which command?", commandMenuOptions);
    if (selectedCommandIndex > -1)
      processCommand((u32)selectedCommandIndex);
    print();
  }

  if (bHandler->hasBeenPressedNow())
    toggleLogLevel();

  if (lHandler->hasBeenPressedNow()) {
    if (upHandler->getIsPressed())
      scrollToTop();
    else
      scrollPageUp();
  }

  if (rHandler->hasBeenPressedNow()) {
    if (downHandler->getIsPressed())
      scrollToBottom();
    else
      scrollPageDown();
  }

  if (upHandler->getIsPressed())
    scrollBack();

  if (downHandler->getIsPressed())
    scrollForward();

  if (selectHandler->hasBeenPressedNow())
    clear();

  if (startHandler->hasBeenPressedNow())
    resetAdapter();
}

void DebugScene::toggleLogLevel() {
  if (useVerboseLog) {
    useVerboseLog = false;
    log("! setting log level to NORMAL");
  } else {
    useVerboseLog = true;
    log("! setting log level to VERBOSE");
  }
  log("");
}

int DebugScene::selectOption(std::string title,
                             std::vector<std::string> options) {
  u32 selectedOption = 0;
  bool firstTime = true;

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;
    processKeys(keys);

    u32 oldOption = selectedOption;

    if (lHandler->hasBeenPressedNow()) {
      if (upHandler->getIsPressed())
        selectedOption = 0;
      else
        selectedOption = max(selectedOption - MAX_LINES, 0);
    }
    if (rHandler->hasBeenPressedNow()) {
      if (downHandler->getIsPressed())
        selectedOption = options.size() - 1;
      else
        selectedOption = min(selectedOption + MAX_LINES, options.size() - 1);
    }
    if (downHandler->hasBeenPressedNow() && selectedOption < options.size() - 1)
      selectedOption++;
    if (upHandler->hasBeenPressedNow() && selectedOption > 0)
      selectedOption--;

    if (firstTime || selectedOption != oldOption) {
      TextStream::instance().setText(title, 0, -3);
      printScrollableText(selectedOption, options, true);
    }

    if (bHandler->hasBeenPressedNow())
      return -1;

    if (aHandler->hasBeenPressedNow())
      return selectedOption;

    VBlankIntrWait();
    firstTime = false;
  }
}

std::string DebugScene::selectString(u32 maxCharacters) {
  std::vector<std::string> options = {"<end>"};
  for (u32 i = 0; i < CHARACTERS_LEN; i++)
    options.push_back(CHARACTERS[i]);

again:
  std::string str;
  for (u32 i = 0; i < maxCharacters; i++) {
    int characterIndex = -1;
    if ((characterIndex =
             selectOption("Next character? (" + str + ")", options)) == -1) {
      if (i == 0)
        return "";
      else
        goto again;
    }
    if (characterIndex == 0)
      break;
    str += CHARACTERS[characterIndex - 1];
  }

  if (str == "")
    goto again;

  if (selectOption(">> " + str + "?", std::vector<std::string>{"yes", "no"}) ==
      1)
    goto again;

  return str;
}

int DebugScene::selectU16() {
again:
  int lsB = selectU8("Choose lsB (0x00XX)");
  if (lsB == -1)
    return -1;
  int msB = selectU8("Choose msB (0xXX" + linkRawWireless->toHex(lsB, 2) + ")");
  if (msB == -1)
    goto again;

  u16 number = linkRawWireless->buildU16((u8)msB, (u8)lsB);
  if (selectOption(">> 0x" + linkRawWireless->toHex(number, 4) + "?",
                   std::vector<std::string>{"yes", "no"}) == 1)
    goto again;

  return number;
}

int DebugScene::selectU8(std::string title) {
  std::vector<std::string> options;
  for (u32 i = 0; i < 1 << 8; i++)
    options.push_back(linkRawWireless->toHex(i, 2));
  return selectOption(title, options);
}

void DebugScene::processCommand(u32 selectedCommandIndex) {
  std::string selectedCommand = commandMenuOptions[selectedCommandIndex];

  if (selectedCommand == "0x10 (Hello)") {
    logSimpleCommand(selectedCommand, 0x11);
  } else if (selectedCommand == "0x11 (SignalLevel)") {
    logSimpleCommand(selectedCommand, 0x11);
  } else if (selectedCommand == "0x12 (VersionStatus)") {
    logSimpleCommand(selectedCommand, 0x12);
  } else if (selectedCommand == "0x13 (SystemStatus)") {
    logSimpleCommand(selectedCommand, 0x13);
  } else if (selectedCommand == "0x14 (SlotStatus)") {
    logOperation("sending " + selectedCommand, []() {
      LinkRawWireless::SlotStatusResponse response;
      bool success = linkRawWireless->getSlotStatus(response);
      log("< [next slot] " +
          linkRawWireless->toHex(response.nextClientNumber, 2));
      for (u32 i = 0; i < response.connectedClients.size(); i++) {
        log("< [client" +
            std::to_string(response.connectedClients[i].clientNumber) + "] " +
            linkRawWireless->toHex(response.connectedClients[i].deviceId, 4));
      }
      return success;
    });
  } else if (selectedCommand == "0x15 (ConfigStatus)") {
    logSimpleCommand(selectedCommand, 0x15);
  } else if (selectedCommand == "0x16 (Broadcast)") {
    int gameId = selectGameId();
    if (gameId == -1)
      return;
    std::string gameName = selectGameName();
    if (gameName == "")
      return;
    std::string userName = selectUserName();
    if (userName == "")
      return;

    logOperation("sending " + selectedCommand, [gameName, userName, gameId]() {
      return linkRawWireless->broadcast(gameName, userName, (u16)gameId);
    });
  } else if (selectedCommand == "0x17 (Setup)") {
    int maxPlayers = -1;
    while ((maxPlayers = selectOption(
                "Max players?",
                std::vector<std::string>{"5", "4", "3", "2"})) == -1)
      ;

    logOperation("sending " + selectedCommand, [maxPlayers]() {
      return linkRawWireless->setup(5 - maxPlayers);
    });
  }
}

int DebugScene::selectGameId() {
  switch (selectOption(
      "GameID?",
      std::vector<std::string>{"0x7FFF", "0x1234", "<random>", "<pick>"})) {
    case 0: {
      return 0x7fff;
    }
    case 1: {
      return 0x1234;
    }
    case 2: {
      return linkRawWireless->buildU16(qran_range(0, 256), qran_range(0, 256));
    }
    default: {
      return selectU16();
    }
  }
}

std::string DebugScene::selectGameName() {
  switch (selectOption("Game name?",
                       std::vector<std::string>{"LinkConnection", "<pick>"})) {
    case 0: {
      return "LinkConnection";
    }
    default: {
      return selectString(LINK_RAW_WIRELESS_MAX_GAME_NAME_LENGTH);
    }
  }
}

std::string DebugScene::selectUserName() {
  switch (
      selectOption("User name?", std::vector<std::string>{"Demo", "<pick>"})) {
    case 0: {
      return "Demo";
    }
    default: {
      return selectString(LINK_RAW_WIRELESS_MAX_USER_NAME_LENGTH);
    }
  }
}

void DebugScene::logSimpleCommand(std::string name, u32 id) {
  logOperation("sending " + name, [id]() {
    auto result = linkRawWireless->sendCommand(id);
    for (u32 i = 0; i < result.responses.size(); i++) {
      log("< [response" + std::to_string(i) + "] " +
          linkRawWireless->toHex(result.responses[i]));
    }
    return result.success;
  });
}

void DebugScene::logOperation(std::string name,
                              std::function<bool()> operation) {
  log("> " + name + "...");
  bool success = operation();
  log(success ? "< success :)" : "< failure :(");
  log("");
}

void DebugScene::resetAdapter() {
  logOperation("resetting adapter",
               []() { return linkRawWireless->activate(); });
}
