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
//       interrupt_init();
//       interrupt_add(INTR_VBLANK, LINK_PS2_KEYBOARD_ISR_VBLANK);
//       interrupt_add(INTR_SERIAL, LINK_PS2_KEYBOARD_ISR_SERIAL);
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

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_PS2_KEYBOARD_VERSION = "vLinkPS2Keyboard/v8.0.0";

/**
 * @brief A PS/2 Keyboard Adapter for the GBA.
 */
class LinkPS2Keyboard {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;

  static constexpr int RCNT_GPIO_AND_SI_IRQ = 0b1000000100000000;
  static constexpr int RCNT_GPIO = 0b1000000000000000;
  static constexpr int SI_DIRECTION = 0b1000000;
  static constexpr int SO_DIRECTION = 0b10000000;
  static constexpr int SI_DATA = 0b100;
  static constexpr int SO_DATA = 0b1000;
  static constexpr int TIMEOUT_FRAMES = 15;  // (~250ms)

  LinkPS2Keyboard() = delete;

 public:
  using EventCallback = void (*)(u8 event);

  /**
   * @brief Constructs a new LinkPS2Keyboard object.
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
    LINK_READ_TAG(LINK_PS2_KEYBOARD_VERSION);

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
        for (u32 i = 0; i < 8; i++)
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
  F1 = 5,
  F2 = 6,
  F3 = 4,
  F4 = 12,
  F5 = 3,
  F6 = 11,
  F7 = 131,
  F8 = 10,
  F9 = 1,
  F10 = 9,
  F11 = 120,
  F12 = 7,
  BACKSPACE = 102,
  TAB = 13,
  ENTER = 90,
  SHIFT_L = 18,
  SHIFT_R = 89,
  SUPER = 97,
  CTRL_L = 20,
  SPECIAL_CTRL_R = 224 + 20,
  ALT_L = 17,
  SPECIAL_ALT_R = 224 + 17,
  SPACE = 41,
  CAPS_LOCK = 88,
  NUM_LOCK = 119,
  SCROLL_LOCK = 126,
  SPECIAL_INSERT = 224 + 112,
  SPECIAL_DELETE = 224 + 113,
  SPECIAL_HOME = 224 + 108,
  SPECIAL_END = 224 + 105,
  SPECIAL_PAGE_UP = 224 + 125,
  SPECIAL_PAGE_DOWN = 224 + 122,
  SPECIAL_UP = 224 + 117,
  SPECIAL_DOWN = 224 + 114,
  SPECIAL_LEFT = 224 + 107,
  SPECIAL_RIGHT = 224 + 116,
  A = 28,
  B = 50,
  C = 33,
  D = 35,
  E = 36,
  F = 43,
  G = 52,
  H = 51,
  I = 67,
  J = 59,
  K = 66,
  L = 75,
  M = 58,
  N = 49,
  O = 68,
  P = 77,
  Q = 21,
  R = 45,
  S = 27,
  T = 44,
  U = 60,
  V = 42,
  W = 29,
  X = 34,
  Y = 53,
  Z = 26,
  NUMPAD_0 = 112,
  NUMPAD_1 = 105,
  NUMPAD_2 = 114,
  NUMPAD_3 = 122,
  NUMPAD_4 = 107,
  NUMPAD_5 = 115,
  NUMPAD_6 = 116,
  NUMPAD_7 = 108,
  NUMPAD_8 = 117,
  NUMPAD_9 = 125,
  NUMPAD_PLUS = 121,
  NUMPAD_MINUS = 123,
  SPECIAL_NUMPAD_ENTER = 224 + 90,
  NUMPAD_DOT = 113,
  NUMPAD_ASTERISK = 124,
  NUMPAD_SLASH = 74
};

/**
 * @brief Event Scan Code list.
 */
enum LINK_PS2_KEYBOARD_EVENT {
  SELF_TEST_PASSED = 0xAA,  // Triggered when hot-plugging the keyboard
  RELEASE = 240,            // Triggered before each key release
  SPECIAL = 224             // Triggered before special keys
};

#endif  // LINK_PS2_KEYBOARD_H
