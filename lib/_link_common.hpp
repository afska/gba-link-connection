#ifndef LINK_COMMON_H
#define LINK_COMMON_H

/**
 * @brief This namespace contains shared code between all libraries.
 * \warning Most of these things are borrowed from libtonc.
 */
namespace Link {

// Types

using u32 = unsigned int;
using u16 = unsigned short;
using u8 = unsigned char;

using vs32 = volatile signed int;
using vu32 = volatile unsigned int;
using s16 = signed short;
using s8 = signed char;

// Structs

struct _TMR_REC {
  union {
    u16 start;
    u16 count;
  } __attribute__((packed));

  u16 cnt;
} __attribute__((aligned(4)));

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

// I/O Registers

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

inline volatile u16& _REG_KEYS =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0130);

inline volatile u16& _REG_TM1CNT_L =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0104);

inline volatile u16& _REG_TM1CNT_H =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0106);

inline volatile u16& _REG_TM2CNT_L =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x0108);

inline volatile u16& _REG_TM2CNT_H =
    *reinterpret_cast<volatile u16*>(_REG_BASE + 0x010a);

inline volatile _TMR_REC* const _REG_TM =
    reinterpret_cast<volatile _TMR_REC*>(_REG_BASE + 0x0100);

static constexpr u16 _KEY_ANY = 0x03FF;       //!< Here's the Any key :)
static constexpr u16 _TM_FREQ_1 = 0;          //!< 1 cycle/tick (16.7 MHz)
static constexpr u16 _TM_FREQ_64 = 0x0001;    //!< 64 cycles/tick (262 kHz)
static constexpr u16 _TM_FREQ_256 = 0x0002;   //!< 256 cycles/tick (66 kHz)
static constexpr u16 _TM_FREQ_1024 = 0x0003;  //!< 1024 cycles/tick (16 kHz)
static constexpr u16 _TM_CASCADE =
    0x0004;  //!< Increment when preceding timer overflows
static constexpr u16 _TM_IRQ = 0x0040;     //!< Enable timer irq
static constexpr u16 _TM_ENABLE = 0x0080;  //!< Enable timer

static constexpr u16 _IRQ_VBLANK = 0x0001;  //!< Catch VBlank irq
static constexpr u16 _IRQ_TIMER0 = 0x0008;  //!< Catch timer 0 irq
static constexpr u16 _IRQ_TIMER1 = 0x0010;  //!< Catch timer 1 irq
static constexpr u16 _IRQ_TIMER2 = 0x0020;  //!< Catch timer 2 irq
static constexpr u16 _IRQ_TIMER3 = 0x0040;  //!< Catch timer 3 irq
static constexpr u16 _IRQ_SERIAL = 0x0080;  //!< Catch serial comm irq
static constexpr u16 _TIMER_IRQ_IDS[] = {_IRQ_TIMER0, _IRQ_TIMER1, _IRQ_TIMER2,
                                         _IRQ_TIMER3};

// SWI

static inline __attribute__((always_inline)) void _IntrWait(u32 flagClear,
                                                            u32 irq) {
  register u32 r0 asm("r0") = flagClear;
  register u32 r1 asm("r1") = irq;

  asm volatile("swi 0x04\n" : : "r"(r0), "r"(r1));
}

static inline __attribute__((always_inline)) int _MultiBoot(_MultiBootParam* mb,
                                                            u32 mode) {
  register _MultiBootParam* r0 asm("r0") = mb;
  register u32 r1 asm("r1") = mode;

  asm volatile("swi 0x25\n" : "+r"(r0) : "r"(r1));

  return (int)r0;
}

// Helpers

static inline int _max(int a, int b) {
  return (a > b) ? (a) : (b);
}

static inline int _min(int a, int b) {
  return (a < b) ? (a) : (b);
}

template <typename T>
struct DefaultNullValue {
  static constexpr T value = T{};
};

template <typename T, u32 Size, T NullValue = 0, bool Overwrite = true>
class Queue {
 public:
  void push(T item) {
    if (isFull()) {
      if constexpr (Overwrite) {
        pop();
      } else {
        return;
      }
    }

    rear = (rear + 1) % Size;
    arr[rear] = item;
    count++;
  }

  T pop() {
    if (isEmpty())
      return NullValue;

    auto x = arr[front];
    front = (front + 1) % Size;
    count--;

    return x;
  }

  T peek() {
    if (isEmpty())
      return NullValue;
    return arr[front];
  }

  template <typename F>
  void forEach(F action) {
    vs32 currentFront = front;

    for (u32 i = 0; i < count; i++) {
      if (!action(arr[currentFront]))
        return;
      currentFront = (currentFront + 1) % Size;
    }
  }

  void clear() {
    front = count = 0;
    rear = -1;
  }

  u32 size() { return count; }
  bool isEmpty() { return size() == 0; }
  bool isFull() { return size() == Size; }

 private:
  T arr[Size];
  vs32 front = 0;
  vs32 rear = -1;
  vu32 count = 0;
};

}  // namespace Link

#endif  // LINK_COMMON_H
