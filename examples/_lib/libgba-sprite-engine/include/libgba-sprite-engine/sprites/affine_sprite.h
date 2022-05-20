//
// Created by Wouter Groeneveld on 28/07/18.
//

#ifndef GBA_SPRITE_ENGINE_AFFINE_SPRITE_H
#define GBA_SPRITE_ENGINE_AFFINE_SPRITE_H

#pragma GCC system_header

#include "sprite.h"

class SpriteManager;

class AffineSprite : public Sprite {
 private:
  int affIndex;
  std::unique_ptr<OBJ_AFFINE> affine;
  void setTransformationMatrix(OBJ_AFFINE* matrix);

  void rebuildOamAttr1ForAffineIndex();

 protected:
  void buildOam(int tileIndex) override;
  void syncOam() override;

 public:
  void setAffineIndex(int index) { this->affIndex = index; }
  void identity();
  void rotate(u16 alpha);
  explicit AffineSprite(const AffineSprite& other);
  explicit AffineSprite(const void* imgData,
                        int imgSize,
                        int xC,
                        int yC,
                        SpriteSize spriteSize);
  OBJ_AFFINE* getMatrix() { return affine.get(); }

  friend class SpriteManager;
};

#endif  // GBA_SPRITE_ENGINE_AFFINE_SPRITE_H
