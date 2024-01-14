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

  void addCommandMenuOptions();
  void processKeys(u16 keys);
  void processButtons();
  void toggleLogLevel();
  int selectOption(std::string title, std::vector<std::string> options);
  std::string selectString(u32 maxCharacters);
  u16 selectU16();
  int selectU8(std::string title);
  void processCommand(u32 selectedCommandIndex);
  u16 selectGameId();
  std::string selectGameName();
  std::string selectUserName();
  void logSimpleCommand(std::string name, u32 id);
  void logOperation(std::string name, std::function<bool()> operation);
  void resetAdapter();
};

#endif  // DEBUG_SCENE_H
