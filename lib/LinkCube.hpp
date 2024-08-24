#ifndef LINK_CUBE_H
#define LINK_CUBE_H

// --------------------------------------------------------------------------
// A JOYBUS handler for the Link Port.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCube* linkCube = new LinkCube();
// - 2) Add the required interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_SERIAL, LINK_CUBE_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkCube->activate();
// // TODO: WRITE
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#include "_link_common.hpp"

/**
 * @brief // TODO: WRITE
 */
#define LINK_CUBE_QUEUE_SIZE 10

static volatile char LINK_CUBE_VERSION[] = "LinkCube/v7.0.0";

#define LINK_CUBE_BARRIER asm volatile("" ::: "memory")  // TODO: USE?

#if LINK_ENABLE_DEBUG_LOGS != 0
#define _LCLOG_(...) Link::log(__VA_ARGS__)
#else
#define _LCLOG_(...)
#endif

/**
 * @brief A JOYBUS handler for the Link Port.
 */
class LinkCube {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;
  using U32Queue = Link::Queue<u16, LINK_CUBE_QUEUE_SIZE>;

  static constexpr int DEVICE_GBA = 0x0004;
  static constexpr int COMMAND_RESET = 0xff;
  // static constexpr int COMMAND_INFO = 0x00; // TODO: DOESN'T MATTER
  // static constexpr int COMMAND_DATA_WRITE = 0x15;
  // static constexpr int COMMAND_DATA_READ = 0x14;
  static constexpr int BIT_CMD_RESET = 0;
  static constexpr int BIT_CMD_RECEIVE = 1;
  static constexpr int BIT_CMD_SEND = 2;
  static constexpr int COMMAND_DATA_READ = 0x14;
  static constexpr int BIT_IRQ = 6;
  static constexpr int BIT_JOYBUS_HIGH = 14;
  static constexpr int BIT_GENERAL_PURPOSE_LOW = 14;
  static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

 public:
  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library.
   */
  void activate() {
    LINK_CUBE_BARRIER;
    isEnabled = false;
    LINK_CUBE_BARRIER;

    resetState();
    stop();

    LINK_CUBE_BARRIER;
    isEnabled = true;
    LINK_CUBE_BARRIER;

    start();

    Link::log("ACTIVATED!");
  }

 public:
  int n = 0;  // TODO: REMOVE

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    resetState();
    stop();
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    n++;

    if (!isEnabled)
      return;

    if (isBitHigh(BIT_CMD_RESET)) {
      Link::log("RESET HIGH?", Link::_REG_JOY_RECV_H);
      Link::log("RESET LOW?", Link::_REG_JOY_RECV_L);
      return;
    }
    if (isBitHigh(BIT_CMD_RECEIVE)) {
      // TODO: RECEIVE
      return;
    }
    if (isBitHigh(BIT_CMD_SEND)) {
      // TODO: RECEIVE
      return;
    }
  }

 private:
  U32Queue incomingQueue;  // TODO: SYNCHRONIZE?
  U32Queue outgoingQueue;
  volatile bool isEnabled = false;

  void resetState() {
    incomingQueue.clear();
    outgoingQueue.clear();
  }

  void stop() {
    setInterruptsOff();
    setGeneralPurposeMode();
  }

  void start() {
    setJoybusMode();
    setInterruptsOn();
    _LCLOG_("Activated irq");  // TODO: REMOVE
  }

  void setJoybusMode() {
    Link::_REG_RCNT = Link::_REG_RCNT | (1 << BIT_JOYBUS_HIGH) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  void setInterruptsOn() { setBitHigh(BIT_IRQ); }
  void setInterruptsOff() { setBitLow(BIT_IRQ); }

  // TODO: REMOVE?
  static u32 buildU32(u8 msB, u8 byte2, u8 byte3, u8 lsB) {
    return ((msB & 0xFF) << 24) | ((byte2 & 0xFF) << 16) |
           ((byte3 & 0xFF) << 8) | (lsB & 0xFF);
  }
  static u16 buildU16(u8 msB, u8 lsB) { return (msB << 8) | lsB; }
  static u16 msB32(u32 value) { return value >> 16; }
  static u16 lsB32(u32 value) { return value & 0xffff; }
  static u8 msB16(u16 value) { return value >> 8; }
  static u8 lsB16(u16 value) { return value & 0xff; }
  bool isBitHigh(u8 bit) { return (Link::_REG_JOYCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { Link::_REG_JOYCNT |= 1 << bit; }
  void setBitLow(u8 bit) { Link::_REG_JOYCNT &= ~(1 << bit); }
};

extern LinkCube* linkCube;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_CUBE_ISR_SERIAL() {
  linkCube->_onSerial();
}

#undef _LCLOG_

#endif  // LINK_CUBE_H
