#define LINK_RAW_WIRELESS_ENABLE_LOGGING

#include "../../../lib/LinkRawWireless.hpp"

#include <libgba-sprite-engine/gba_engine.h>
#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"
#include "scenes/DebugScene.h"

void setUpInterrupts();
void printTutorial();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
static std::unique_ptr<DebugScene> debugScene{new DebugScene(engine)};

LinkRawWireless* linkRawWireless = new LinkRawWireless();

int main() {
  setUpInterrupts();

  engine->setScene(debugScene.get());

  while (true) {
    engine->update();

    VBlankIntrWait();
  }

  return 0;
}

inline void setUpInterrupts() {
  interrupt_init();

  interrupt_set_handler(INTR_VBLANK, [] {});
  interrupt_enable(INTR_VBLANK);

  // A+B+START+SELECT = SoftReset
#if MULTIBOOT_BUILD == 0
  REG_KEYCNT = 0b1100000000001111;
  interrupt_set_handler(INTR_KEYPAD, Common::ISR_reset);
  interrupt_enable(INTR_KEYPAD);
#endif
}
