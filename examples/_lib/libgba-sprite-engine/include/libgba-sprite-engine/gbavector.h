//
// Created by Wouter Groeneveld on 14/12/18.
//

#ifndef GBA_SPRITE_ENGINE_PROJECT_MATH_H
#define GBA_SPRITE_ENGINE_PROJECT_MATH_H

#pragma GCC system_header

#include <deque>

#ifdef CODE_COMPILED_AS_PART_OF_TEST
#include <libgba-sprite-engine/gba/tonc_math_stub.h>
#else
#include <libgba-sprite-engine/gba/tonc_math.h>
#endif

#include <string>

class GBAVector {
 private:
  VECTOR v;

 public:
  GBAVector() : v({}) {}
  GBAVector(VECTOR v) : v(v) {}

  std::deque<VECTOR> bresenhamLineTo(VECTOR dest);
  VECTOR rotateAsCenter(VECTOR point, uint angle);

  std::string to_string() {
    return "(" + std::to_string(v.x) + "," + std::to_string(v.y) + ")";
  }
};

#endif  // GBA_SPRITE_ENGINE_PROJECT_MATH_H
