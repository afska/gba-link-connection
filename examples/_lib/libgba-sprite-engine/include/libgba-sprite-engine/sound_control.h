//
// Created by Wouter Groeneveld on 07/08/18.
//

#ifndef GBA_SPRITE_ENGINE_SOUND_H
#define GBA_SPRITE_ENGINE_SOUND_H

#pragma GCC system_header

#include <libgba-sprite-engine/gba/tonc_memdef.h>
#include <libgba-sprite-engine/gba/tonc_memmap.h>
#include <libgba-sprite-engine/gba/tonc_types.h>
#include <memory>

#define CLOCK 16777216
#define CYCLES_PER_BLANK 280806
#define OVERFLOW_16_BIT_VALUE 65536
#define DISPLAY_INTERRUPT_VBLANK_ENABLE 0x08
#define INTERRUPT_VBLANK 0x1
#define DMA_SYNC_TO_TIMER 0x30000000

#define IRQ_CALLBACK ((volatile unsigned int*)0x3007FFC)

enum SoundChannel { ChannelA, ChannelB };

class SoundControl {
 private:
  vu32* DMAControl;             // ex. &REG_DMA1CNT
  vu32* DMASourceAddress;       // ex. &REG_DMA1SAD
  vu32* DMADestinationAddress;  // ex. &REG_DMA1DAD
  vu32* FiFoBuffer;             // ex. &REG_FIFOA
  u16 controlFlags;
  u32 vblanksRemaning;  // updated each vblank, counts down to 0
  u32 vblanksTotal;     // calculated once when enqueueing

  const void* data;

 public:
  SoundControl(vu32* dma, vu32* src, vu32* dest, vu32* fifo, u16 flags)
      : DMAControl(dma),
        DMASourceAddress(src),
        DMADestinationAddress(dest),
        FiFoBuffer(fifo),
        controlFlags(flags),
        vblanksRemaning(0),
        vblanksTotal(0) {}

  u16 getControlFlags() { return controlFlags; }
  u32 getVBlanksRemaning() { return vblanksRemaning; }
  void reset();
  void step() { vblanksRemaning--; }
  bool done() { return vblanksRemaning <= 0; }
  u32 getVBlanksTotal() { return vblanksTotal; }

  void disable() {
    *(DMAControl) = 0;
    vblanksRemaning = 0;
    REG_SNDDSCNT &= ~(controlFlags);
  };
  void enable() {
    *DMAControl =
        DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_SYNC_TO_TIMER | DMA_ENABLE;
  };
  void accept(const void* data, int totalSamples, int ticksPerSample);

  static std::unique_ptr<SoundControl> channelAControl();
  static std::unique_ptr<SoundControl> channelBControl();

  static std::unique_ptr<SoundControl> soundControl(SoundChannel channel) {
    return channel == ChannelA ? channelAControl() : channelBControl();
  };
};

#endif  // GBA_SPRITE_ENGINE_SOUND_H
