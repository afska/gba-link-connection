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
static std::unique_ptr<InputHandler> lHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> rHandler =
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

void log(std::string string) {
  debugLines.push_back(string);
  currentDebugLine = debugLines.size() - 1;
  print();
}

void scrollBack() {
  if (currentDebugLine < 19)
    return;
  currentDebugLine--;
  print();
}

void scrollForward() {
  if (currentDebugLine == (int)debugLines.size() - 1)
    return;
  currentDebugLine++;
  print();
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

  log("Hello");
  log("Line 1");
  log("Line 2");
  log("Line 3");
  log("Line 4");
  log("Line 5");
  log("Line 6");
  log("Line 7");
  log("Line 8");
  log("Line 9");
  log("Line 10");
  log("Line 11");
  log("Line 12");
  log("Line 13");
  log("Line 14");
  log("Line 16");
  log("Line 17");
  log("Line 18");
  log("Line 19");
  log("Line 20");
  log("Line 21");
  log("Line 22");
}

void DebugScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  startHandler->setIsPressed(keys & KEY_START);

  if (lHandler->getIsPressed())
    scrollBack();

  if (rHandler->getIsPressed())
    scrollForward();
}
