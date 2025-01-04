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
  struct CommandMenuOption {
    std::string name;
    u8 command;
  };
  std::vector<CommandMenuOption> commandMenuOptions;
  u16 serverIds[4];
  int lastSelectedCommandIndex = 0;

  void addCommandMenuOptions();
  void processKeys(u16 keys);
  void processButtons();
  void toggleLogLevel();
  int selectOption(std::string title,
                   std::vector<std::string> options,
                   u32 cursor);
  std::string selectString(u32 maxCharacters);
  int selectU32(std::string title);
  int selectU16();
  int selectU8(std::string title);
  void processCommand(u32 selectedCommandIndex);
  int selectServerId();
  int selectGameId();
  std::string selectGameName();
  std::string selectUserName();
  std::vector<u32> selectDataToSend();
  std::vector<u32> selectData();
  void logGenericWaitCommand(std::string name, u32 id);
  void logGenericCommand(std::string name, u32 id);
  void logSimpleCommand(std::string name, u32 id, std::vector<u32> params = {});
  void logOperation(std::string name, std::function<bool()> operation);
  void resetAdapter();
  void restoreExistingConnection();
};

#endif  // DEBUG_SCENE_H
