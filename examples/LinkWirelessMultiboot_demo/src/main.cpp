#include "../../../lib/LinkWirelessMultiboot.hpp"

#include <libgba-sprite-engine/gba_engine.h>
#include "../../_lib/common.h"
#include "../../_lib/interrupt.h"
#include "../../_lib/libgba-sprite-engine/scene.h"
#include "scenes/MultibootScene.h"

void setUpInterrupts();
void printTutorial();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
static std::unique_ptr<MultibootScene> multibootScene{
    new MultibootScene(engine)};

LinkWirelessMultiboot* linkWirelessMultiboot = new LinkWirelessMultiboot();

int main() {
  setUpInterrupts();

  engine->setScene(multibootScene.get());

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
  REG_KEYCNT = 0b1100000000001111;
  interrupt_set_handler(INTR_KEYPAD, Common::ISR_reset);
  interrupt_enable(INTR_KEYPAD);
}
