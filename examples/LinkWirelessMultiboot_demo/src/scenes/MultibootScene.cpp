// (0) Include the header
#include "../../../../lib/LinkWirelessMultiboot.hpp"

#include "MultibootScene.h"

#include <libgba-sprite-engine/background/text_stream.h>
#include <tonc.h>
#include <cstring>
#include <functional>

#include "utils/InputHandler.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "../../_lib/libgbfs/gbfs.h"
}

static const GBFS_FILE* fs = find_first_gbfs_file(0);

MultibootScene::MultibootScene(std::shared_ptr<GBAEngine> engine)
    : Scene(engine) {}

static std::unique_ptr<InputHandler> aHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> bHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> leftHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> rightHandler =
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
static u32 selectedFile = 0;
static u32 players = 5;

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

void printFile() {
  char name[32];
  gbfs_get_nth_obj(fs, selectedFile, name, NULL);
  for (u32 i = 0; i < 32; i++) {
    if (name[i] == '.') {
      name[i] = '\0';
      break;
    }
  }

  log("! selected file: " + std::to_string(selectedFile + 1) + "/" +
      std::to_string(fs->dir_nmemb));
  log("  " + std::string(name));
}

void selectLeft() {
  if (selectedFile == 0)
    return printFile();
  selectedFile--;
  printFile();
}

void selectRight() {
  if ((int)selectedFile >= fs->dir_nmemb - 1)
    return printFile();
  selectedFile++;
  printFile();
}

void launchROM() {
  log("Launching...");
  VBlankIntrWait();

  REG_IME = 0;

  u32 fileLength;
  const u8* romToSend =
      (const u8*)gbfs_get_nth_obj(fs, selectedFile, NULL, &fileLength);

  void* EWRAM = (void*)0x02000000;
  std::memcpy(EWRAM, romToSend, fileLength);

  asm volatile(
      "mov r0, %0\n"
      "bx r0\n"
      :
      : "r"(EWRAM)
      : "r0");

  while (true)
    ;
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

#ifdef LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING
  linkWirelessMultiboot->logger = [](std::string string) { log(string); };
#endif

#ifdef LINK_RAW_WIRELESS_ENABLE_LOGGING
  linkWirelessMultiboot->linkRawWireless->logger = [](std::string string) {
    log(string);
  };
#endif

  log("---");
  log("LinkWirelessMultiboot demo");
  log("  (v7.1.0)");
  log("");
  if (fs == NULL) {
    log("! GBFS file not found");
    while (true)
      ;
  } else if (gbfs_get_nth_obj(fs, 0, NULL, NULL) == NULL) {
    log("! No files found (GBFS)");
    while (true)
      ;
  }
  log("A: send ROM");
  log("B: launch ROM");
  log("START: toggle players");
  log("LEFT/RIGHT: select ROM");
  log("UP/DOWN: scroll up/down");
  log("L/R: scroll page up/down");
  log("UP+L/DOWN+R: scroll top/bottom");
  log("L+R: cancel transfer");
  log("SELECT: clear");
  log("---");
  log("");
  togglePlayers();
  selectedFile = fs->dir_nmemb - 1;
  printFile();
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
  leftHandler->setIsPressed(keys & KEY_LEFT);
  rightHandler->setIsPressed(keys & KEY_RIGHT);
  upHandler->setIsPressed(keys & KEY_UP);
  downHandler->setIsPressed(keys & KEY_DOWN);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);
  startHandler->setIsPressed(keys & KEY_START);
}

void MultibootScene::processButtons() {
  if (startHandler->hasBeenPressedNow())
    togglePlayers();

  if (aHandler->hasBeenPressedNow()) {
    u32 fileLength;
    const u8* romToSend =
        (const u8*)gbfs_get_nth_obj(fs, selectedFile, NULL, &fileLength);

    clear();
    printFile();

    // (2) Send the ROM
    auto result = linkWirelessMultiboot->sendRom(
        romToSend, fileLength, "Multiboot", "Test", 0xffff, players,
        [](LinkWirelessMultiboot::MultibootProgress progress) {
          u16 keys = ~REG_KEYS & KEY_ANY;
          if (keys & KEY_SELECT) {
            // SELECT = Start transfer before the player count is reached
            *progress.ready = true;
          }
          return (keys & KEY_L) && (keys & KEY_R);
        },
        true);
    log("-> result: " + std::to_string(result));
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

  if (leftHandler->hasBeenPressedNow())
    selectLeft();

  if (rightHandler->hasBeenPressedNow())
    selectRight();

  if (upHandler->getIsPressed())
    scrollBack();

  if (downHandler->getIsPressed())
    scrollForward();

  if (selectHandler->hasBeenPressedNow())
    clear();

  if (bHandler->hasBeenPressedNow())
    launchROM();
}

void MultibootScene::togglePlayers() {
  players++;
  if (players > LINK_WIRELESS_MULTIBOOT_MAX_PLAYERS)
    players = LINK_WIRELESS_MULTIBOOT_MIN_PLAYERS;

  log("! setting players: " + std::to_string(players));
}

void MultibootScene::logOperation(std::string name,
                                  std::function<bool()> operation) {
  log("> " + name + "...");
  bool success = operation();
  log(success ? "< success :)" : "< failure :(");
  log("");
}
