#ifndef LINK_COMMON_H
#define LINK_COMMON_H

/**
 * @brief This namespace contains the parts of libtonc used by this library.
 */
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

typedef struct {
  u32 reserved1[5];
  u8 handshake_data;
  u8 padding;
  u16 handshake_timeout;
  u8 probe_count;
  u8 client_data[3];
  u8 palette_data;
  u8 response_bit;
  u8 client_bit;
  u8 reserved2;
  u8* boot_srcp;
  u8* boot_endp;
  u8* masterp;
  u8* reserved3[3];
  u32 system_work2[4];
  u8 sendflag;
  u8 probe_target_bit;
  u8 check_wait;
  u8 server_type;
} _MultiBootParam;

inline int _MultiBoot(_MultiBootParam* mb, u32 mode) {
  int result;
  asm volatile(
      "mov r0, %1\n"        // mb => r0
      "mov r1, %2\n"        // mode => r1
      "swi 0x25\n"          // call 0x25
      "mov %0, r0\n"        // r0 => output
      : "=r"(result)        // output
      : "r"(mb), "r"(mode)  // inputs
      : "r0", "r1"          // clobbered registers
  );
  return result;
}

}  // namespace Link

#endif  // LINK_COMMON_H
