#ifndef LINK_EXAMPLES_COMMON_H
#define LINK_EXAMPLES_COMMON_H

#include <tonc.h>
#include <string>

namespace Common {

// TTE

inline void initTTE() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

inline void log(std::string text) {
  tte_erase_screen();
  tte_write("#{P:0,0}");
  tte_write(text.c_str());
}

// BIOS

inline void ISR_reset() {
  REG_IME = 0;
  RegisterRamReset(RESET_REG | RESET_VRAM);
#if MULTIBOOT_BUILD == 1
  *(vu8*)0x03007FFA = 0x01;
#endif
  SoftReset();
}

// Input

inline bool didPress(u16 key, bool& pressed) {
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

inline void waitForKey(u16 key) {
  u16 keys;
  do {
    keys = ~REG_KEYS & KEY_ANY;
  } while (!(keys & key));
}

// Profiling

inline u32 toMs(u32 cycles) {
  // CPU Frequency * time per frame = cycles per frame
  // 16780000 * (1/60) ~= 279666
  return (cycles * 1000) / (279666 * 60);
}

inline void profileStart() {
  REG_TM1CNT_L = 0;
  REG_TM2CNT_L = 0;

  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  REG_TM2CNT_H = TM_ENABLE | TM_CASCADE;
  REG_TM1CNT_H = TM_ENABLE | TM_FREQ_1;
}

inline u32 profileStop() {
  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  return (REG_TM1CNT_L | (REG_TM2CNT_L << 16));
}

// Bits

inline bool isBitHigh(u16 data, u8 bit) {
  return (data >> bit) & 1;
}

}  // namespace Common

#endif  // LINK_EXAMPLES_COMMON_H
