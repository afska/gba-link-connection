// FULL:
// This example has a menu and lets the user send data in different ways.

#include "main.h"
#include <libgba-sprite-engine/gba_engine.h>
#include "../../_lib/interrupt.h"
#include "scenes/TestScene.h"
#include "utils/SceneUtils.h"

void setUpInterrupts();
void printTutorial();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
static std::unique_ptr<TestScene> testScene{new TestScene(engine)};

#ifndef USE_LINK_UNIVERSAL
LinkCable* linkCable = new LinkCable();
LinkCable* linkConnection = linkCable;
#else
LinkUniversal* linkUniversal = new LinkUniversal();
LinkUniversal* linkConnection = linkUniversal;
#endif

int main() {
  setUpInterrupts();

  engine->setScene(testScene.get());

  printTutorial();

  while (true) {
    u16 keys = ~REG_KEYS & KEY_ANY;

    // enable and disable
    if ((keys & KEY_DOWN) && linkConnection->isActive()) {
      linkConnection->deactivate();
      DEBULOG("! stopped");
    }
    if ((keys & KEY_START) && !linkConnection->isActive()) {
      linkConnection->activate();
      DEBULOG("! started");
    }

    static constexpr int BIT_READY = 3;
    static constexpr int BIT_ERROR = 6;
    static constexpr int BIT_START = 7;

    // log player ID/count and important flags
    TextStream::instance().setText(
        "P" + asStr(linkConnection->currentPlayerId()) + "/" +
            asStr(linkConnection->playerCount()) + "-R" +
            asStr(isBitHigh(REG_SIOCNT, BIT_READY)) + "-S" +
            asStr(isBitHigh(REG_SIOCNT, BIT_ERROR)) + "-E" +
            asStr(isBitHigh(REG_SIOCNT, BIT_START)),
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

#ifndef USE_LINK_UNIVERSAL
  // LinkCable
  interrupt_set_handler(INTR_VBLANK, LINK_CABLE_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CABLE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_CABLE_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#else
  // LinkUniversal
  interrupt_set_handler(INTR_VBLANK, LINK_UNIVERSAL_ISR_VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);
#endif

  // A+B+START+SELECT
  REG_KEYCNT = 0b1100000000001111;
  interrupt_set_handler(INTR_KEYPAD, ISR_reset);
  interrupt_enable(INTR_KEYPAD);
}

void printTutorial() {
#ifndef USE_LINK_UNIVERSAL
  DEBULOG("LinkCable_full (v7.1.0)");
#else
  DEBULOG("LinkUniversal_full (v7.1.0)");
#endif

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
