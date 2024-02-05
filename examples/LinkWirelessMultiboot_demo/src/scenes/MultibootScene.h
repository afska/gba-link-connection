#ifndef MULTIBOOT_SCENE_H
#define MULTIBOOT_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include <string>
#include <vector>

class MultibootScene : public Scene {
 public:
  MultibootScene(std::shared_ptr<GBAEngine> engine);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;

 private:
  struct CommandMenuOption {
    std::string name;
    u8 command;
  };
  int lastSelectedCommandIndex = 0;

  void processKeys(u16 keys);
  void processButtons();
  void togglePlayers();
  void logOperation(std::string name, std::function<bool()> operation);
};

#endif  // MULTIBOOT_SCENE_H
