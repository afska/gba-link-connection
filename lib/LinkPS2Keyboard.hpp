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
// (*1) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//      That causes packet loss. You REALLY want to use libugba's instead.
//      (see examples)
// --------------------------------------------------------------------------
// (*2) The hardware is very sensitive to timing. Make sure that
//      `LINK_PS2_KEYBOARD_ISR_SERIAL()` is handled on time. That means:
//      Be careful with DMA usage (which stops the CPU), and write short
//      interrupt handlers (or activate nested interrupts by setting
//      `REG_IME=1` at the start of your handlers).
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

#include "_link_common.hpp"

static volatile char LINK_PS2_KEYBOARD_VERSION[] = "LinkPS2Keyboard/v7.0.0";

/**
 * @brief A PS/2 Keyboard Adapter for the GBA.
 */
class LinkPS2Keyboard {
 private:
  using EventCallback = void (*)(u8 event);
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int RCNT_GPIO_AND_SI_IRQ = 0b1000000100000000;
  static constexpr int RCNT_GPIO = 0b1000000000000000;
  static constexpr int SI_DIRECTION = 0b1000000;
  static constexpr int SO_DIRECTION = 0b10000000;
  static constexpr int SI_DATA = 0b100;
  static constexpr int SO_DATA = 0b1000;
  static constexpr int TIMEOUT_FRAMES = 15;  // (~250ms)

 public:
  /**
   * @brief Constructs a new LinkPS2Keyboard object.
   *
   * @param onEvent Function pointer that will receive the scan codes (`u8`).
   * Check out `LINK_PS2_KEYBOARD_KEY` and `LINK_PS2_KEYBOARD_EVENT` for codes.
   */
  explicit LinkPS2Keyboard(EventCallback onEvent) { this->onEvent = onEvent; }

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   */
  void activate() {
    deactivate();

    Link::_REG_RCNT = RCNT_GPIO_AND_SI_IRQ;
    Link::_REG_SIOCNT = 0;

    bitcount = 0;
    incoming = 0;
    parityBit = 0;
    prevFrame = 0;
    frameCounter = 0;

    isEnabled = true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;

    Link::_REG_RCNT = RCNT_GPIO;
    Link::_REG_SIOCNT = 0;
  }

  /**
   * @brief This method is called by the VBLANK interrupt handler.
   * \warning This is internal API!
   */
  void _onVBlank() { frameCounter++; }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled)
      return;

    u8 val = (Link::_REG_RCNT & SO_DATA) != 0;

    u32 nowFrame = frameCounter;
    if (nowFrame - prevFrame > TIMEOUT_FRAMES) {
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
  EventCallback onEvent;
};

extern LinkPS2Keyboard* linkPS2Keyboard;

/**
 * @brief VBLANK interrupt handler.
 */
inline void LINK_PS2_KEYBOARD_ISR_VBLANK() {
  if (!linkPS2Keyboard->isActive())
    return;

  linkPS2Keyboard->_onVBlank();
}

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_PS2_KEYBOARD_ISR_SERIAL() {
  linkPS2Keyboard->_onSerial();
}

/**
 * @brief Key Scan Code list.
 */
enum LINK_PS2_KEYBOARD_KEY {
  ESC = 118,
  F1 = 112,
  F2 = 121,
  F3 = 123,
  F4 = 125,
  F5 = 130,
  F6 = 137,
  F7 = 139,
  F8 = 143,
  F9 = 145,
  F10 = 148,
  F11 = 153,
  F12 = 156,
  BACKSPACE = 102,
  TAB = 26,
  ENTER = 90,
  SHIFT = 18,
  CTRL = 17,
  ALT = 20,
  SPACE = 33,
  CAPS_LOCK = 58,
  NUM_LOCK = 90,
  SCROLL_LOCK = 119,
  INSERT = 99,
  DELETE = 113,
  HOME = 103,
  END = 107,
  PAGE_UP = 104,
  PAGE_DOWN = 109,
  UP = 117,
  DOWN = 119,
  LEFT = 107,
  RIGHT = 116,
  A = 28,
  B = 50,
  C = 33,
  D = 30,
  E = 36,
  F = 34,
  G = 35,
  H = 43,
  I = 44,
  J = 45,
  K = 46,
  L = 47,
  M = 50,
  N = 49,
  O = 48,
  P = 49,
  Q = 21,
  R = 38,
  S = 27,
  T = 40,
  U = 42,
  V = 32,
  W = 24,
  X = 29,
  Y = 41,
  Z = 26,
  NUMPAD_0 = 104,
  NUMPAD_1 = 105,
  NUMPAD_2 = 110,
  NUMPAD_3 = 122,
  NUMPAD_4 = 108,
  NUMPAD_5 = 115,
  NUMPAD_6 = 111,
  NUMPAD_7 = 108,
  NUMPAD_8 = 114,
  NUMPAD_9 = 125,
  NUMPAD_PLUS = 121,
  NUMPAD_MINUS = 123,
  NUMPAD_ENTER = 96,
  NUMPAD_DOT = 99,
  NUMPAD_ASTERISK = 55,
  NUMPAD_SLASH = 53
};

/**
 * @brief Event Scan Code list.
 */
enum LINK_PS2_KEYBOARD_EVENT {
  RELEASE = 240,  // Triggered before each key release
  SPECIAL = 224   // Triggered before special keys
};

#endif  // LINK_PS2_KEYBOARD_H