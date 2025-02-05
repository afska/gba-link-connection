#ifndef LINK_EXAMPLES_COMMON_H
#define LINK_EXAMPLES_COMMON_H

#include <tonc.h>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief This namespace contains shared code between all the examples.
 */
namespace Common {

inline std::string lastLog = "";

// Strings

static inline int firstDiffLine(std::string_view oldText,
                                std::string_view newText) {
  int line = 0;
  u32 oldPos = 0, newPos = 0;

  while (true) {
    u32 oldNL = oldText.find('\n', oldPos);
    u32 newNL = newText.find('\n', newPos);

    auto oldLine = (oldNL == std::string_view::npos)
                       ? oldText.substr(oldPos)
                       : oldText.substr(oldPos, oldNL - oldPos);
    auto newLine = (newNL == std::string_view::npos)
                       ? newText.substr(newPos)
                       : newText.substr(newPos, newNL - newPos);
    if (oldLine != newLine)
      return line;
    if (oldNL == std::string_view::npos && newNL == std::string_view::npos)
      break;
    oldPos = (oldNL == std::string_view::npos) ? oldText.size() : oldNL + 1;
    newPos = (newNL == std::string_view::npos) ? newText.size() : newNL + 1;
    line++;
  }

  return -1;
}

static inline int firstDiffLine(const std::vector<std::string>& oldLines,
                                const std::vector<std::string>& newLines) {
  int maxLines = Link::_max(oldLines.size(), newLines.size());
  for (int i = 0; i < maxLines; ++i) {
    if (i >= (int)oldLines.size() || i >= (int)newLines.size() ||
        oldLines[i] != newLines[i])
      return i;
  }
  return -1;
}

// TTE

static inline void initTTE() {
  REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
  tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
}

static inline void log(const std::string& text) {
  int diffLine = firstDiffLine(lastLog, text);
  if (diffLine == -1)
    return;

  tte_erase_rect(0, diffLine * 8, 240, 160);
  tte_write(
      (std::string("#{P:0,") + std::to_string(diffLine * 8) + "}").c_str());

  u32 pos = 0;
  for (int i = 0; i < diffLine; ++i) {
    pos = text.find('\n', pos);
    if (pos == std::string::npos) {
      pos = text.size();
      break;
    }
    pos++;
  }
  tte_write(text.substr(pos).c_str());
  lastLog = text;
}

// BIOS

static inline void ISR_reset() {
  REG_IME = 0;
  RegisterRamReset(RESET_REG | RESET_VRAM);
#if MULTIBOOT_BUILD == 1
  *(vu8*)0x03007FFA = 0x01;
#endif
  SoftReset();
}

// Input

static inline bool didPress(u16 key, bool& pressed) {
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

static inline void waitForKey(u16 key) {
  u16 keys;
  do {
    keys = ~REG_KEYS & KEY_ANY;
  } while (!(keys & key));
}

// Profiling

static inline u32 toMs(u32 cycles) {
  // CPU Frequency * time per frame = cycles per frame
  // 16780000 * (1/60) ~= 279666
  return (cycles * 1000) / (279666 * 60);
}

static inline void profileStart() {
  REG_TM1CNT_L = 0;
  REG_TM2CNT_L = 0;

  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  REG_TM2CNT_H = TM_ENABLE | TM_CASCADE;
  REG_TM1CNT_H = TM_ENABLE | TM_FREQ_1;
}

static inline u32 profileStop() {
  REG_TM1CNT_H = 0;
  REG_TM2CNT_H = 0;

  return (REG_TM1CNT_L | (REG_TM2CNT_L << 16));
}

// Bits

static inline bool isBitHigh(u16 data, u8 bit) {
  return (data >> bit) & 1;
}

}  // namespace Common

#endif  // LINK_EXAMPLES_COMMON_H
