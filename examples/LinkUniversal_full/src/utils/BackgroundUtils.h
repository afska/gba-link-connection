#ifndef BACKGROUND_UTILS_H
#define BACKGROUND_UTILS_H

#include <tonc_memdef.h>
#include <tonc_memmap.h>

inline void BACKGROUND_enable(bool bg0, bool bg1, bool bg2, bool bg3) {
  REG_DISPCNT = bg0 ? REG_DISPCNT | DCNT_BG0 : REG_DISPCNT & ~DCNT_BG0;
  REG_DISPCNT = bg1 ? REG_DISPCNT | DCNT_BG1 : REG_DISPCNT & ~DCNT_BG1;
  REG_DISPCNT = bg2 ? REG_DISPCNT | DCNT_BG2 : REG_DISPCNT & ~DCNT_BG2;
  REG_DISPCNT = bg3 ? REG_DISPCNT | DCNT_BG3 : REG_DISPCNT & ~DCNT_BG3;
}

#endif  // BACKGROUND_UTILS_H
