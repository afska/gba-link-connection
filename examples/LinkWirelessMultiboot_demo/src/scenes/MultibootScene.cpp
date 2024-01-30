#include "MultibootScene.h"

#include <libgba-sprite-engine/background/text_stream.h>
#include <tonc.h>
#include <algorithm>
#include <functional>

#include "../../../../lib/LinkWirelessMultiboot.hpp"
#include "utils/InputHandler.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

static const GBFS_FILE* fs = find_first_gbfs_file(0);

const char* ROM_FILE_NAME = "rom-to-transfer.gba";

MultibootScene::MultibootScene(std::shared_ptr<GBAEngine> engine)
    : Scene(engine) {}

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

static std::vector<std::string> logLines;
static u32 currentLogLine = 0;
static bool useVerboseLog = true;

#define MAX_LINES 20
#define DRAW_LINE 0

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

std::vector<Background*> MultibootScene::backgrounds() {
  return {};
}

std::vector<Sprite*> MultibootScene::sprites() {
  std::vector<Sprite*> sprites;
  return sprites;
}

void MultibootScene::load() {
  SCENE_init();
  BACKGROUND_enable(true, false, false, false);

  linkWirelessMultiboot->logger = [](std::string string) {
    // if (useVerboseLog) // TODO: RESTORE
    log(string);
  };
  linkWirelessMultiboot->link->logger = [](std::string string) {
    if (useVerboseLog)
      log(string);
  };

  log("---");
  log("LinkWirelessMultiboot demo");
  log("  (v6.2.0)");
  log("");
  if (fs == NULL) {
    log("! GBFS file not found");
    while (true)
      ;
  } else if (gbfs_get_obj(fs, ROM_FILE_NAME, NULL) == NULL) {
    log("! File not found in GBFS:");
    log("  " + std::string(ROM_FILE_NAME));
    while (true)
      ;
  }
  log("A: send ROM");
  log("B: toggle log level");
  log("UP/DOWN: scroll up/down");
  log("L/R: scroll page up/down");
  log("UP+L/DOWN+R: scroll to top/bottom");
  log("SELECT: clear");
  log("---");
  log("");
  toggleLogLevel();
}

void MultibootScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  processKeys(keys);
  processButtons();

  __qran_seed += keys;
  __qran_seed += REG_RCNT;
  __qran_seed += REG_SIOCNT;
}

void MultibootScene::processKeys(u16 keys) {
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  upHandler->setIsPressed(keys & KEY_UP);
  downHandler->setIsPressed(keys & KEY_DOWN);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);
}

void MultibootScene::processButtons() {
  if (bHandler->hasBeenPressedNow())
    toggleLogLevel();

  if (aHandler->hasBeenPressedNow()) {
    u32 fileLength;
    const u8* romToSend =
        (const u8*)gbfs_get_obj(fs, ROM_FILE_NAME, &fileLength);

    linkWirelessMultiboot->sendRom(romToSend, fileLength, "Multi", "Test", 2,
                                   []() { return false; });
    print();
  }

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
}

void MultibootScene::toggleLogLevel() {
  if (useVerboseLog) {
    useVerboseLog = false;
    log("! setting log level to NORMAL");
  } else {
    useVerboseLog = true;
    log("! setting log level to VERBOSE");
  }
  log("");
}

void MultibootScene::logOperation(std::string name,
                                  std::function<bool()> operation) {
  log("> " + name + "...");
  bool success = operation();
  log(success ? "< success :)" : "< failure :(");
  log("");
}
