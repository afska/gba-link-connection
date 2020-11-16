#include "TestScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "../lib/LinkConnection.h"
#include "utils/InputHandler.h"
#include "utils/SceneUtils.h"

TestScene::TestScene(std::shared_ptr<GBAEngine> engine) : Scene(engine) {}

static std::unique_ptr<InputHandler> aHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> bHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> lHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> rHandler =
    std::unique_ptr<InputHandler>(new InputHandler());
static std::unique_ptr<InputHandler> selectHandler =
    std::unique_ptr<InputHandler>(new InputHandler());

inline void send(u16 data) {
  DEBULOG("-> " + asStr(data));
  linkConnection->send(data);
}

std::vector<Background*> TestScene::backgrounds() {
  return {};
}

std::vector<Sprite*> TestScene::sprites() {
  std::vector<Sprite*> sprites;
  return sprites;
}

void TestScene::load() {
  SCENE_init();
  BACKGROUND_enable(true, false, false, false);
}

void TestScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  frameCounter++;

  // check keys
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);

  // log events
  auto linkState = linkConnection->linkState.get();
  if (!isConnected && linkState->isConnected()) {
    isConnected = true;
    initialized = false;
    DEBULOG("! connected (" + asStr(linkState->playerCount) + " players)");
  }
  if (isConnected && !linkState->isConnected()) {
    isConnected = false;
    DEBULOG("! disconnected");
  }
  if (selectHandler->hasBeenPressedNow()) {
    DEBULOG("! lagging...");
    SCENE_wait(9000);
  }

  // determine which value should be sent
  u16 value = LINK_NO_DATA;
  if (!initialized && linkConnection->linkState->currentPlayerId == 1) {
    initialized = true;
    value = 999;
  }
  if (aHandler->getIsPressed())
    value = 555;
  if (bHandler->hasBeenPressedNow()) {
    counter++;
    value = counter;
  }

  if (linkState->isConnected() && linkState->currentPlayerId == 0)
    linkConnection->send(10000);

  // send data
  if (lHandler->hasBeenPressedNow()) {
    send(1);
    send(2);
  } else if (rHandler->hasBeenPressedNow()) {
    send(43981);
    send(257);
  } else if (value != LINK_NO_DATA)
    send(value);

  // process received data
  if (linkState->isConnected())
    for (u32 i = 0; i < linkState->playerCount; i++)
      while (linkState->hasMessage(i)) {
        u16 message = linkState->readMessage(i);
        if (i != linkState->currentPlayerId && message != 10000)
          DEBULOG("<-p" + asStr(i) + ": " + asStr(message) + " (frame " +
                  asStr(frameCounter) + ")");
      }
}
