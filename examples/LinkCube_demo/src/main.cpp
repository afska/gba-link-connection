#define LINK_ENABLE_DEBUG_LOGS 1  // TODO: 0

// (0) Include the header
#include "../../../lib/LinkCube.hpp"

#include <tonc.h>
#include <string>
#include "../../_lib/interrupt.h"

void log(std::string text);
void wait(u32 verticalLines);
bool didPress(unsigned short key, bool& pressed);
template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len = sizeof(I) << 1);
inline void VBLANK() {}

// (1) Create a LinkCube instance
LinkCube* linkCube = new LinkCube();

void init() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));

  // (2) Add the interrupt service routines
  interrupt_init();
  interrupt_set_handler(INTR_VBLANK, VBLANK);
  interrupt_enable(INTR_VBLANK);
  interrupt_set_handler(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
}

int main() {
  init();

  // (3) Initialize the library
  linkCube->activate();

  int counter = -1;
  std::string received = "";

  while (true) {
    // Title
    std::string output =  // TODO: IMPLEMENT pending
        "LinkCube_demo (v7.0.0)\n\nPress A to send\nPress B to clear\n\nLast "
        "sent: " +
        std::to_string(counter) + "\n(pending = " + std::to_string(0) +
        ")\n\nReceived:\n" + received;

    output += std::to_string(Link::_REG_JOY_RECV_H) + ", \n";
    output += std::to_string(Link::_REG_JOY_RECV_L) + ", \n";
    output += "joycnt: " + std::to_string(Link::_REG_JOYCNT) + ", \n";
    output += "joystat: " + std::to_string(Link::_REG_JOYSTAT) + ", \n";

    // TODO: IMPLEMENT transfers

    // Print
    VBlankIntrWait();
    log(output);
  }

  return 0;
}

void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}

void wait(u32 verticalLines) {
  u32 count = 0;
  u32 vCount = REG_VCOUNT;

  while (count < verticalLines) {
    if (REG_VCOUNT != vCount) {
      count++;
      vCount = REG_VCOUNT;
    }
  };
}

bool didPress(u16 key, bool& pressed) {
  u16 keys = ~REG_KEYS & KEY_ANY;
  bool isPressedNow = false;
  if ((keys & key) && !pressed) {
    pressed = true;
    isPressedNow = true;
  }
  if (pressed && !(keys & key))
    pressed = false;
  return isPressedNow;
}

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len) {
  static const char* digits = "0123456789ABCDEF";
  std::string rc(hex_len, '0');
  for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
    rc[i] = digits[(w >> j) & 0x0f];
  return rc;
}
