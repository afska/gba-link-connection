#ifndef LINK_PS2_KEYBOARD_H
#define LINK_PS2_KEYBOARD_H

// --------------------------------------------------------------------------
// A PS/2 Keyboard Adapter for the GBA.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkPS2Keyboard* linkPS2Keyboard = new LinkPS2Keyboard([](u8 event) {
//         // handle event (check example scan codes below)
//       });
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_VBLANK, LINK_PS2_KEYBOARD_ISR_VBLANK);
//       irq_add(II_SERIAL, LINK_PS2_KEYBOARD_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkPS2Keyboard->activate();
// - 4) Handle events in the callback sent to LinkPS2Keyboard's constructor!
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
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

// Example Scan Codes: (Num Lock OFF)
#define LINK_PS2_KEYBOARD_KEY_Z 26              // Z
#define LINK_PS2_KEYBOARD_KEY_Q 21              // Q
#define LINK_PS2_KEYBOARD_KEY_S 27              // S
#define LINK_PS2_KEYBOARD_KEY_E 36              // E
#define LINK_PS2_KEYBOARD_KEY_C 33              // C
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_1 105      // Numpad 1
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_7 108      // Numpad 7
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_5 115      // Numpad 5
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_9 125      // Numpad 9
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_3 122      // Numpad 3
#define LINK_PS2_KEYBOARD_KEY_ENTER 90          // Enter
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_PLUS 121   // Numpad +
#define LINK_PS2_KEYBOARD_KEY_BACKSPACE 102     // Backspace
#define LINK_PS2_KEYBOARD_KEY_NUMPAD_MINUS 123  // Numpad -
#define LINK_PS2_KEYBOARD_KEY_LEFT 107          // Left
#define LINK_PS2_KEYBOARD_KEY_RIGHT 116         // Right
#define LINK_PS2_KEYBOARD_KEY_UP 117            // Up
#define LINK_PS2_KEYBOARD_KEY_ESC 118           // ESC
#define LINK_PS2_KEYBOARD_KEY_SUPR 113          // Supr
// ---
#define LINK_PS2_KEYBOARD_KEY_RELEASE 240  // Triggered before each key release
#define LINK_PS2_KEYBOARD_KEY_SPECIAL 224  // Triggered before special keys

#define LINK_PS2_KEYBOARD_SI_DIRECTION 0b1000000
#define LINK_PS2_KEYBOARD_SO_DIRECTION 0b10000000
#define LINK_PS2_KEYBOARD_SI_DATA 0b100
#define LINK_PS2_KEYBOARD_SO_DATA 0b1000
#define LINK_PS2_KEYBOARD_TIMEOUT_FRAMES 15  // (~250ms)

static volatile char LINK_PS2_KEYBOARD_VERSION[] = "LinkPS2Keyboard/v7.0.0";

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
    parityBit = 0;
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
    if (!isEnabled)
      return;

    u8 val = (REG_RCNT & LINK_PS2_KEYBOARD_SO_DATA) != 0;

    u32 nowFrame = frameCounter;
    if (nowFrame - prevFrame > LINK_PS2_KEYBOARD_TIMEOUT_FRAMES) {
      bitcount = 0;
      incoming = 0;
      parityBit = 0;
    }
    prevFrame = nowFrame;

    if (bitcount == 0 && val == 0) {  // start bit detected
      // start bit is always 0, so only proceed if val is 0
      bitcount++;
    } else if (bitcount >= 1 && bitcount <= 8) {  // data bits
      incoming |= (val << (bitcount - 1));
      bitcount++;
    } else if (bitcount == 9) {  // parity bit
      // store parity bit for later check
      parityBit = val;
      bitcount++;
    } else if (bitcount == 10) {  // stop bit
      if (val == 1) {             // stop bit should be 1
        // calculate parity (including the stored parity bit from previous IRQ)
        u8 parity = 0;
        for (u8 i = 0; i < 8; i++)
          parity += (incoming >> i) & 1;
        parity += parityBit;

        if (parity % 2 != 0)  // odd parity as expected
          onEvent(incoming);
      }
      bitcount = 0;
      incoming = 0;
      parityBit = 0;
    }
  }

 private:
  bool isEnabled = false;
  u8 bitcount = 0;
  u8 incoming = 0;
  u8 parityBit = 0;
  u32 prevFrame = 0;
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
  linkPS2Keyboard->_onSerial();
}

#endif  // LINK_PS2_KEYBOARD_H