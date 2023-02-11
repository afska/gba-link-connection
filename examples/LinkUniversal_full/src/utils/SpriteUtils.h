#ifndef SPRITE_UTILS_H
#define SPRITE_UTILS_H

#include <tonc_memdef.h>
#include <tonc_memmap.h>

inline void SPRITE_disable() {
  REG_DISPCNT = REG_DISPCNT & ~DCNT_OBJ;
}

#endif  // SPRITE_UTILS_H
