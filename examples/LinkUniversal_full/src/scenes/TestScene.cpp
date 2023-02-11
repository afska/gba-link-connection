#include "TestScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "../../../lib/LinkUniversal.h"
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
  if (!linkUniversal->isConnected())
    return;

  DEBULOG("-> " + asStr(data));
  linkUniversal->send(data);
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

  // sync
  linkUniversal->sync();

  // check keys
  aHandler->setIsPressed(keys & KEY_A);
  bHandler->setIsPressed(keys & KEY_B);
  lHandler->setIsPressed(keys & KEY_L);
  rHandler->setIsPressed(keys & KEY_R);
  selectHandler->setIsPressed(keys & KEY_SELECT);

  // log events
  if (!isConnected && linkUniversal->isConnected()) {
    isConnected = true;
    initialized = false;
    DEBULOG("! connected (" + asStr(linkUniversal->playerCount()) +
            " players)");
  }
  if (isConnected && !linkUniversal->isConnected()) {
    isConnected = false;
    DEBULOG("! disconnected");
  }
  if (selectHandler->hasBeenPressedNow()) {
    DEBULOG("! lagging...");
    SCENE_wait(9000);
  }

  // determine which value should be sent
  u16 value = LINK_CABLE_NO_DATA;
  if (!initialized && linkUniversal->isConnected() &&
      linkUniversal->currentPlayerId() == 1) {
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
  if (linkUniversal->isConnected()) {
    for (u32 i = 0; i < linkUniversal->playerCount(); i++) {
      while (linkUniversal->canRead(i)) {
        u16 message = linkUniversal->read(i);
        if (i != linkUniversal->currentPlayerId())
          DEBULOG("<-p" + asStr(i) + ": " + asStr(message) + " (frame " +
                  asStr(frameCounter) + ")");
      }
    }
  }
}
