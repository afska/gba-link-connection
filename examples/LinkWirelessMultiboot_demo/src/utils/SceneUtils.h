#ifndef SCENE_UTILS_H
#define SCENE_UTILS_H

#include <libgba-sprite-engine/background/text_stream.h>
#include <tonc_memdef.h>
#include <tonc_memmap.h>
#include <string>

const u32 TEXT_MIDDLE_COL = 12;

inline std::string asStr(u16 data) {
  return std::to_string(data);
}

inline void BACKGROUND_enable(bool bg0, bool bg1, bool bg2, bool bg3) {
  REG_DISPCNT = bg0 ? REG_DISPCNT | DCNT_BG0 : REG_DISPCNT & ~DCNT_BG0;
  REG_DISPCNT = bg1 ? REG_DISPCNT | DCNT_BG1 : REG_DISPCNT & ~DCNT_BG1;
  REG_DISPCNT = bg2 ? REG_DISPCNT | DCNT_BG2 : REG_DISPCNT & ~DCNT_BG2;
  REG_DISPCNT = bg3 ? REG_DISPCNT | DCNT_BG3 : REG_DISPCNT & ~DCNT_BG3;
}

inline void SPRITE_disable() {
  REG_DISPCNT = REG_DISPCNT & ~DCNT_OBJ;
}

inline void SCENE_init() {
  TextStream::instance().clear();
  TextStream::instance().scroll(0, 0);
  TextStream::instance().setMosaic(false);

  BACKGROUND_enable(false, false, false, false);
  SPRITE_disable();
}

inline void SCENE_write(std::string text, u32 row) {
  TextStream::instance().setText(text, row,
                                 TEXT_MIDDLE_COL - text.length() / 2);
}

#endif  // SCENE_UTILS_H
