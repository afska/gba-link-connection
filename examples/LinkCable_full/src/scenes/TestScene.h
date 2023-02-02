#ifndef TEST_SCENE_H
#define TEST_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include <string>

class TestScene : public Scene {
 public:
  TestScene(std::shared_ptr<GBAEngine> engine);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;

 private:
  u32 counter = 0;
  bool isConnected = false;
  bool initialized = false;
  u32 frameCounter = 0;
};

#endif  // TEST_SCENE_H
