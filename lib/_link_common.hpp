#ifndef LINK_COMMON_H
#define LINK_COMMON_H

/**
 * @brief Enable mGBA debug logging.
 */
#ifndef LINK_ENABLE_DEBUG_LOGS
#define LINK_ENABLE_DEBUG_LOGS 0
#endif

#if LINK_ENABLE_DEBUG_LOGS != 0
#include <stdarg.h>
#include <stdio.h>
#endif

/**
 * @brief This namespace contains shared code between all libraries.
 * \warning Most of these things are borrowed from libtonc and gba-hpp.
 */
namespace Link {

// Types

using u32 = unsigned int;
using u16 = unsigned short;
using u8 = unsigned char;

using s16 = signed short;
using s8 = signed char;

using vu32 = volatile unsigned int;
using vs32 = volatile signed int;
using vu16 = volatile unsigned short;
using vs16 = volatile signed short;

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

inline vu16& _REG_RCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0134);
inline vu16& _REG_SIOCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0128);
inline vu32& _REG_SIODATA32 = *reinterpret_cast<vu32*>(_REG_BASE + 0x0120);
inline vu16& _REG_SIODATA8 = *reinterpret_cast<vu16*>(_REG_BASE + 0x012A);
inline vu16& _REG_SIOMLT_SEND = *reinterpret_cast<vu16*>(_REG_BASE + 0x012A);
inline vu16* const _REG_SIOMULTI = reinterpret_cast<vu16*>(_REG_BASE + 0x0120);
inline vu16& _REG_JOYCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0140);
inline vu16& _REG_JOY_RECV_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0150);
inline vu16& _REG_JOY_RECV_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0152);
inline vu16& _REG_JOY_TRANS_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0154);
inline vu16& _REG_JOY_TRANS_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0156);
inline vu16& _REG_JOYSTAT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0158);
inline vu16& _REG_VCOUNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0006);
inline vu16& _REG_KEYS = *reinterpret_cast<vu16*>(_REG_BASE + 0x0130);
inline vu16& _REG_TM1CNT_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0104);
inline vu16& _REG_TM1CNT_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0106);
inline vu16& _REG_TM2CNT_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0108);
inline vu16& _REG_TM2CNT_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x010a);
inline vu16& _REG_IME = *reinterpret_cast<vu16*>(_REG_BASE + 0x0208);

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

static inline __attribute__((always_inline)) void _IntrWait(
    bool clearCurrent,
    u32 flags) noexcept {
  register auto r0 asm("r0") = clearCurrent;
  register auto r1 asm("r1") = flags;
  asm volatile inline("swi 0x4 << ((1f - . == 4) * -16); 1:"
                      : "+r"(r0), "+r"(r1)::"r3");
}

static inline auto _MultiBoot(const _MultiBootParam* param,
                              u32 mbmode) noexcept {
  register union {
    const _MultiBootParam* ptr;
    int res;
  } r0 asm("r0") = {param};
  register auto r1 asm("r1") = mbmode;
  asm volatile inline("swi 0x25 << ((1f - . == 4) * -16); 1:"
                      : "+r"(r0), "+r"(r1)::"r3");
  return r0.res;
}

// Helpers

static inline int _max(int a, int b) {
  return (a > b) ? (a) : (b);
}

static inline int _min(int a, int b) {
  return (a < b) ? (a) : (b);
}

// Queue

template <typename T, u32 Size, bool Overwrite = true>
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
      return T{};

    auto x = arr[front];
    front = (front + 1) % Size;
    count--;

    return x;
  }

  T peek() {
    if (isEmpty())
      return T{};
    return arr[front];
  }

  T* peekRef() {
    if (isEmpty())
      return nullptr;
    return &arr[front];
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

  void startReading() { _isReading = true; }
  void stopReading() { _isReading = false; }

  void syncPush(T item) {
    _isWriting = true;
    asm volatile("" ::: "memory");

    push(item);

    asm volatile("" ::: "memory");
    _isWriting = false;
    asm volatile("" ::: "memory");

    if (_needsClear) {
      clear();
      _needsClear = false;
    }
  }

  T syncPop() {
    _isReading = true;
    asm volatile("" ::: "memory");

    auto value = pop();

    asm volatile("" ::: "memory");
    _isReading = false;
    asm volatile("" ::: "memory");

    return value;
  }

  void syncClear() {
    if (_isReading)
      return;  // (it will be cleared later anyway)

    if (!_isWriting)
      clear();
    else
      _needsClear = true;
  }

  u32 size() { return count; }
  bool isEmpty() { return count == 0; }
  bool isFull() { return count == Size; }
  bool isReading() { return _isReading; }
  bool isWriting() { return _isWriting; }
  bool canMutate() { return !_isReading && !_isWriting; }

 private:
  T arr[Size];
  vs32 front = 0;
  vs32 rear = -1;
  vu32 count = 0;
  volatile bool _isReading = false;
  volatile bool _isWriting = false;
  volatile bool _needsClear = false;
};

// Packets per frame -> Timer interval
static inline u16 perFrame(u16 packets) {
  return (1667 * 1024) / (packets * 6104);
}

// mGBA Logging

#if LINK_ENABLE_DEBUG_LOGS != 0
inline vu16& _REG_LOG_ENABLE = *reinterpret_cast<vu16*>(0x4FFF780);
inline vu16& _REG_LOG_LEVEL = *reinterpret_cast<vu16*>(0x4FFF700);

static inline void log(const char* fmt, ...) {
  _REG_LOG_ENABLE = 0xC0DE;

  va_list args;
  va_start(args, fmt);

  char* const log = (char*)0x4FFF600;
  vsnprintf(log, 0x100, fmt, args);
  _REG_LOG_LEVEL = 0x102;  // Level: WARN

  va_end(args);
}
#endif

}  // namespace Link

#endif  // LINK_COMMON_H
