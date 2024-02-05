#include "TestScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "../main.h"
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

  // sync
  linkConnection->sync();

  frameCounter++;

  // check keys
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);

  // log events
  if (!isConnected && linkConnection->isConnected()) {
    isConnected = true;
    initialized = false;
    DEBULOG("! connected (" + asStr(linkConnection->playerCount()) +
            " players)");
  }
  if (isConnected && !linkConnection->isConnected()) {
    isConnected = false;
    DEBULOG("! disconnected");
  }
  if (selectHandler->hasBeenPressedNow()) {
    DEBULOG("! lagging...");
    SCENE_wait(9000);
  }

  // determine which value should be sent
  u16 value = LINK_CABLE_NO_DATA;
  if (!initialized && linkConnection->isConnected() &&
      linkConnection->currentPlayerId() == 1) {
    initialized = true;
    value = 999;
  }
  if (aHandler->getIsPressed() || bHandler->hasBeenPressedNow()) {
    counter++;
    value = counter;
  }

  // send data
  if (rHandler->getIsPressed() || lHandler->hasBeenPressedNow()) {
    counter++;
    send(counter);
    counter++;
    send(counter);
  } else if (value != LINK_CABLE_NO_DATA) {
    send(value);
  }

  // process received data
  if (linkConnection->isConnected()) {
    for (u32 i = 0; i < linkConnection->playerCount(); i++) {
      while (linkConnection->canRead(i)) {
        u16 message = linkConnection->read(i);
        if (i != linkConnection->currentPlayerId())
          DEBULOG("<-p" + asStr(i) + ": " + asStr(message) + " (frame " +
                  asStr(frameCounter) + ")");
      }
    }
  }
}
