#ifndef SCENE_UTILS_H
#define SCENE_UTILS_H

#include <libgba-sprite-engine/background/text_stream.h>

#include <string>

#include "BackgroundUtils.h"
#include "SpriteUtils.h"

const u32 TEXT_MIDDLE_COL = 12;

extern int DEBULOG_LINE;
void DEBULOG(std::string string);

inline std::string asStr(u16 data) {
  return std::to_string(data);
}

inline bool isBitHigh(u16 data, u8 bit) {
  return (data >> bit) & 1;
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

inline void SCENE_wait(u32 verticalLines) {
  u32 lines = 0;
  u32 vCount = REG_VCOUNT;

  while (lines < verticalLines) {
    if (REG_VCOUNT != vCount) {
      lines++;
      vCount = REG_VCOUNT;
    }
  };
}

#endif  // SCENE_UTILS_H
