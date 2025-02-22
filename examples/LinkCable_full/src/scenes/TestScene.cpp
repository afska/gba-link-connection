#include "TestScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "../main.h"

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
static std::unique_ptr<InputHandler> rightHandler =
    std::unique_ptr<InputHandler>(new InputHandler());

inline void send(u16 data) {
  DEBULOG("-> " + std::to_string(data));
  linkConnection->send(data);
}

void printWirelessSignalLevel();

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
  rightHandler->setIsPressed(keys & KEY_RIGHT);

  // log events
  if (!isConnected && linkConnection->isConnected()) {
    isConnected = true;
    initialized = false;
    DEBULOG("! connected (" + std::to_string(linkConnection->playerCount()) +
            " players)");
  }
  if (isConnected && !linkConnection->isConnected()) {
    isConnected = false;
    DEBULOG("! disconnected");
  }

  // other buttons
  if (selectHandler->getIsPressed()) {
    DEBULOG("! lagging...");
    Link::wait(228 * 5);
  }
  if (rightHandler->hasBeenReleasedNow())
    printWirelessSignalLevel();

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
          DEBULOG("<-p" + std::to_string(i) + ": " + std::to_string(message) +
                  " (frame " + std::to_string(frameCounter) + ")");
      }
    }
  }
}

void printWirelessSignalLevel() {
#ifdef USE_LINK_UNIVERSAL
  if (linkConnection->getMode() != LinkUniversal::Mode::LINK_WIRELESS) {
    DEBULOG("! not in wireless mode");
    return;
  }

  LinkWireless::SignalLevelResponse response;
  if (!linkConnection->getLinkWireless()->getSignalLevel(response)) {
    DEBULOG(linkConnection->getLinkWireless()->getLastError() ==
                    LinkWireless::Error::BUSY_TRY_AGAIN
                ? "! busy, try again"
                : "! failed");
    return;
  }

  if (linkConnection->getLinkWireless()->getState() ==
      LinkWireless::State::SERVING) {
    for (u32 i = 1; i < linkConnection->playerCount(); i++)
      DEBULOG("P" + std::to_string(i) + ": " +
              std::to_string(response.signalLevels[i] * 100 / 255) + "%");
  } else {
    auto playerId = linkConnection->currentPlayerId();
    DEBULOG("P" + std::to_string(playerId) + ": " +
            std::to_string(response.signalLevels[playerId] * 100 / 255) + "%");
  }
#endif
}
