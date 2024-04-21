#ifndef LINK_PS2_KEYBOARD_H
#define LINK_PS2_KEYBOARD_H

// --------------------------------------------------------------------------
// A PS/2 Keyboard Adapter for the GBA
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkPS2Keyboard* linkPS2Keyboard = new LinkPS2Keyboard([](u8 event) {
//         // handle event
//       });
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_PS2_KEYBOARD_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_PS2_KEYBOARD_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkPS2Keyboard->activate();
// - 4) Handle events in the callback sent to LinkPS2Keyboard's constructor!
// --------------------------------------------------------------------------
//  ____________
// |   Pinout   |
// |PS/2 --- GBA|
// |------------|
// |CLOCK -> SI |
// |DATA --> SO |
// |VCC ---> VCC|
// |GND ---> GND|
// --------------------------------------------------------------------------

#include <tonc_core.h>
#include <functional>

#define LINK_PS2_KEYBOARD_SI_DIRECTION 0b1000000
#define LINK_PS2_KEYBOARD_SO_DIRECTION 0b10000000
#define LINK_PS2_KEYBOARD_SI_DATA 0b100
#define LINK_PS2_KEYBOARD_SO_DATA 0b1000
#define LINK_PS2_KEYBOARD_TIMEOUT_FRAMES 15  // (~250ms)

static volatile char LINK_PS2_KEYBOARD_VERSION[] = "LinkPS2Keyboard/v6.3.0";

class LinkPS2Keyboard {
 public:
  explicit LinkPS2Keyboard(std::function<void(u8 event)> onEvent) {
    this->onEvent = onEvent;
  }

  bool isActive() { return isEnabled; }

  void activate() {
    deactivate();

    REG_RCNT = 0b1000000100000000;  // General Purpose Mode + SI interrupts
    REG_SIOCNT = 0;                 // Unused

    bitcount = 0;
    incoming = 0;
    prevFrame = 0;
    frameCounter = 0;

    isEnabled = true;
  }

  void deactivate() {
    isEnabled = false;

    REG_RCNT = 0b1000000000000000;  // General Purpose Mode
    REG_SIOCNT = 0;                 // Unused
  }

  void _onVBlank() { frameCounter++; }

  void _onSerial() {
    u8 val = (REG_RCNT & LINK_PS2_KEYBOARD_SO_DATA) != 0;

    u32 nowFrame = frameCounter;
    if (nowFrame - prevFrame > LINK_PS2_KEYBOARD_TIMEOUT_FRAMES) {
      bitcount = 0;
      incoming = 0;
    }
    prevFrame = nowFrame;

    u8 n = bitcount - 1;
    if (n <= 7)
      incoming |= (val << n);
    bitcount++;

    if (bitcount == 11) {
      onEvent(incoming);

      bitcount = 0;
      incoming = 0;
    }
  }

 private:
  bool isEnabled = false;
  uint8_t bitcount = 0;
  uint8_t incoming = 0;
  uint32_t prevFrame = 0;
  u32 frameCounter = 0;
  std::function<void(u8 event)> onEvent;
};

extern LinkPS2Keyboard* linkPS2Keyboard;

inline void LINK_PS2_KEYBOARD_ISR_VBLANK() {
  if (!linkPS2Keyboard->isActive())
    return;

  linkPS2Keyboard->_onVBlank();
}

inline void LINK_PS2_KEYBOARD_ISR_SERIAL() {
  if (!linkPS2Keyboard->isActive())
    return;

  linkPS2Keyboard->_onSerial();
}

#endif  // LINK_PS2_KEYBOARD_H