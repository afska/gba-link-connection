#ifndef DEBUG_SCENE_H
#define DEBUG_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include <string>
#include <vector>

class DebugScene : public Scene {
 public:
  DebugScene(std::shared_ptr<GBAEngine> engine);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;

 private:
  std::vector<std::string> commandMenuOptions;
  u32 commandMenuSelectedOption = 0;

  void addCommandMenuOptions();
  void processKeys(u16 keys);
  void processButtons();
  void toggleLogLevel();
  void resetAdapter();
  void showCommandSendMenu();
};

#endif  // DEBUG_SCENE_H
