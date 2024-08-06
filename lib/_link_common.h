#ifndef LINK_COMMON_H
#define LINK_COMMON_H

namespace Link {

using u32 = unsigned int;
using u16 = unsigned short;
using u8 = unsigned char;

struct _TMR_REC {
  union {
    u16 start;
    u16 count;
  } __attribute__((packed));

  u16 cnt;
} __attribute__((aligned(4)));

constexpr u32 _REG_BASE = 0x04000000;

inline volatile u16& _REG_RCNT =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0134);

inline volatile u16& _REG_SIOCNT =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0128);

inline volatile u32& _REG_SIODATA32 =
    *reinterpret_cast<volatile u32*>(_REG_BASE + 0x0120);

inline volatile u16& _REG_SIODATA8 =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x012A);

inline volatile u16& _REG_SIOMLT_SEND =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x012A);

inline volatile u16* const _REG_SIOMULTI =
    reinterpret_cast<volatile u16*>(_REG_BASE + 0x0120);

inline volatile u16& _REG_VCOUNT =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0006);

inline volatile _TMR_REC* const _REG_TM =
    reinterpret_cast<volatile _TMR_REC*>(_REG_BASE + 0x0100);

}  // namespace Link

#endif  // LINK_COMMON_H
