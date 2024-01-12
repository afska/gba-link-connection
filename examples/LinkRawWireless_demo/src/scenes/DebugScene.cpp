#include "DebugScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

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

static std::vector<std::string> debugLines;
static int currentDebugLine = 0;

void print() {
  u32 drawLine = 2;
  for (int i = currentDebugLine - 18; i < currentDebugLine; i++) {
    if (i >= 0) {
      TextStream::instance().setText(debugLines[i], drawLine, -3);
      drawLine++;
    }
  }
  for (u32 i = drawLine; i < 20; i++)
    TextStream::instance().setText("                              ", i, -3);
}

void scrollBack() {
  if (currentDebugLine < 19)
    return;
  currentDebugLine--;
  print();
}

void scrollForward() {
  if (currentDebugLine == (int)debugLines.size())
    return;
  currentDebugLine++;
  print();
}

void scrollToTop() {
  currentDebugLine = 18;
  print();
}

void scrollToBottom() {
  currentDebugLine = debugLines.size();
  print();
}

void clear() {
  debugLines.clear();
  currentDebugLine = 0;
  print();
}

void log(std::string string) {
  debugLines.push_back(string);
  scrollToBottom();
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

  log("---");
  log("LinkRawWireless demo");
  log("");
  log("START: reset wireless adapter");
  log("A: send command");
  log("B: toggle log level");
  log("UP/DOWN: scroll up/down");
  log("L/R: scroll to top/bottom");
  log("SELECT: clear");
  log("---");
  log("");
  log("! setting log level to NORMAL");
  log("");
}

void DebugScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  TextStream::instance().setText("state = AUTHENTICATED   p? / ?", 0, -3);

  processButtons(keys);
}

void DebugScene::processButtons(u16 keys) {
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  upHandler->setIsPressed(keys & KEY_UP);
  downHandler->setIsPressed(keys & KEY_DOWN);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);
  startHandler->setIsPressed(keys & KEY_START);

  if (lHandler->hasBeenPressedNow())
    scrollToTop();

  if (rHandler->hasBeenPressedNow())
    scrollToBottom();

  if (upHandler->getIsPressed())
    scrollBack();

  if (downHandler->getIsPressed())
    scrollForward();

  if (selectHandler->hasBeenPressedNow())
    clear();

  if (startHandler->hasBeenPressedNow())
    resetAdapter();
}

void DebugScene::resetAdapter() {
  log("> resetting adapter...");
  bool success = linkRawWireless->activate();
  log(success ? "< it worked :)" : "< it failed :(");
  log("");
}