#include <libgba-sprite-engine/gba_engine.h>
#include <tonc.h>
#include "../../../lib/LinkCable.h"
#include "../../_lib/interrupt.h"
#include "scenes/TestScene.h"
#include "utils/SceneUtils.h"

// FULL:
// This example has a menu and lets the user send data in different ways.

void setUpInterrupts();
void printTutorial();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
static std::unique_ptr<TestScene> testScene{new TestScene(engine)};
LinkCable* linkCable = new LinkCable();

int main() {
  setUpInterrupts();

  engine->setScene(testScene.get());

  printTutorial();

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // enable and disable
    if ((keys & KEY_DOWN) && linkCable->isActive()) {
      linkCable->deactivate();
      DEBULOG("! stopped");
    }
    if ((keys & KEY_START) && !linkCable->isActive()) {
      linkCable->activate();
      DEBULOG("! started");
    }

    // log player id/count and important flags
    TextStream::instance().setText(
        "P" + asStr(linkCable->currentPlayerId()) + "/" +
            asStr(linkCable->playerCount()) + "-R" +
            asStr(isBitHigh(REG_SIOCNT, LINK_CABLE_BIT_READY)) + "-S" +
            asStr(isBitHigh(REG_SIOCNT, LINK_CABLE_BIT_START)) + "-E" +
            asStr(isBitHigh(REG_SIOCNT, LINK_CABLE_BIT_ERROR)),
        0, 14);

    engine->update();

    VBlankIntrWait();
  }

  return 0;
}

inline void ISR_reset() {
  RegisterRamReset(RESET_REG | RESET_VRAM);
  SoftReset();
}

inline void setUpInterrupts() {
  interrupt_init();

  // LinkCable
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // A+B+START+SELECT
  REG_KEYCNT = 0b1100000000001111;
  interrupt_set_handler(INTR_KEYPAD, ISR_reset);
}

void printTutorial() {
  DEBULOG("gba-link-connection demo");
  DEBULOG("");
  DEBULOG("START: turn on connection");
  DEBULOG("(on connection, p1 sends 999)");
  DEBULOG("");
  DEBULOG("B: send counter++ (once)");
  DEBULOG("A: send counter++ (cont)");
  DEBULOG("L: send counter++ twice (once)");
  DEBULOG("R: send counter++ twice (cont)");
  DEBULOG("SELECT: force lag (9k lines)");
  DEBULOG("DOWN: turn off connection");
  DEBULOG("");
}
