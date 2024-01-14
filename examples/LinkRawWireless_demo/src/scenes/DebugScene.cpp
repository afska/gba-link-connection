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
