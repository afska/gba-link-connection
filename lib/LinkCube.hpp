#ifndef LINK_CUBE_H
#define LINK_CUBE_H

// --------------------------------------------------------------------------
// A JOYBUS handler for the Link Port.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCube* linkCube = new LinkCube();
// - 2) Add the required interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_SERIAL, LINK_CUBE_ISR_SERIAL);
// - 3) Initialize the library with:
//       linkCube->activate();
// - 4) Send 32-bit values:
//       linkCube->send(0x12345678);
//       // (now linkCube->pendingCount() will be 1 until the value is sent)
// - 5) Read 32-bit values:
//       if (linkCube->canRead()) {
//         u32 value = linkCube->read();
//         // ...
//       }
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

#ifndef LINK_CUBE_QUEUE_SIZE
/**
 * @brief Buffer size (how many incoming and outgoing values the queues can
 * store at max). The default value is `10`, which seems fine for most games.
 * \warning This affects how much memory is allocated. With the default value,
 * it's around `120` bytes. There's a double-buffered pending queue (to avoid
 * data races), and 1 outgoing queue.
 * \warning You can approximate the usage with `LINK_CUBE_QUEUE_SIZE * 12`.
 */
#define LINK_CUBE_QUEUE_SIZE 10
#endif

static volatile char LINK_CUBE_VERSION[] = "LinkCube/v8.0.0";

/**
 * @brief A JOYBUS handler for the Link Port.
 */
class LinkCube {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using U32Queue = Link::Queue<u32, LINK_CUBE_QUEUE_SIZE>;

  static constexpr int BIT_CMD_RESET = 0;
  static constexpr int BIT_CMD_RECEIVE = 1;
  static constexpr int BIT_CMD_SEND = 2;
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
    LINK_BARRIER;
    isEnabled = false;
    LINK_BARRIER;

    resetState();
    stop();

    LINK_BARRIER;
    isEnabled = true;
    LINK_BARRIER;

    start();
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    resetState();
    stop();
  }

  /**
   * @brief Waits for data. Returns `true` on success, or `false` on
   * JOYBUS reset.
   */
  bool wait() {
    return wait([]() { return false; });
  }

  /**
   * @brief Waits for data. Returns `true` on success, or `false` on
   * JOYBUS reset or cancellation.
   * @param cancel A function that will be invoked after every SERIAL interrupt.
   * If it returns `true`, the wait be aborted.
   * \warning Blocks the system until the next SERIAL interrupt!
   */
  template <typename F>
  bool wait(F cancel) {
    resetFlag = false;

    while (!resetFlag && !canRead() && !cancel())
      Link::_IntrWait(1, Link::_IRQ_SERIAL);

    return canRead();
  }

  /**
   * @brief Returns `true` if there are pending received values to read.
   */
  [[nodiscard]] bool canRead() { return !incomingQueue.isEmpty(); }

  /**
   * @brief Dequeues and returns the next received value.
   * \warning If there's no received data, a `0` will be returned.
   */
  u32 read() { return incomingQueue.syncPop(); }

  /**
   * @brief Returns the next received value without dequeuing it.
   * \warning If there's no received data, a `0` will be returned.
   */
  [[nodiscard]] u32 peek() { return incomingQueue.peek(); }

  /**
   * @brief Sends 32-bit `data`.
   * @param data The value to be sent.
   * \warning If the other end asks for data at the same time you call this
   * method, a `0x00000000` will be sent.
   */
  void send(u32 data) { outgoingQueue.syncPush(data); }

  /**
   * @brief Returns the number of pending outgoing transfers.
   */
  [[nodiscard]] u32 pendingCount() { return outgoingQueue.size(); }

  /**
   * @brief Returns whether the internal receive queue lost messages at some
   * point due to being full. This can happen if your queue size is too low, if
   * you receive too much data without calling `read(...)` enough times, or
   * if excessive `read(...)` calls prevent the ISR from copying data. After
   * this call, the overflow flag is cleared if `clear` is `true` (default
   * behavior).
   */
  [[nodiscard]] bool didQueueOverflow(bool clear = true) {
    bool overflow = newIncomingQueue.overflow;
    if (clear)
      newIncomingQueue.overflow = false;
    return overflow;
  }

  /**
   * @brief Returns whether a JOYBUS reset was requested or not. After this
   * call, the reset flag is cleared if `clear` is `true` (default behavior).
   * @param clear Whether it should clear the reset flag or not.
   */
  bool didReset(bool clear = true) {
    bool reset = resetFlag;
    if (clear)
      resetFlag = false;
    return reset;
  }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled)
      return;

    if (isBitHigh(BIT_CMD_RESET)) {
      resetState();
      resetFlag = true;
      setBitHigh(BIT_CMD_RESET);
    }

    if (isBitHigh(BIT_CMD_RECEIVE)) {
      newIncomingQueue.push(getData());
      setBitHigh(BIT_CMD_RECEIVE);
    }

    if (isBitHigh(BIT_CMD_SEND)) {
      setPendingData();
      setBitHigh(BIT_CMD_SEND);
    }

    copyState();
  }

 private:
  U32Queue newIncomingQueue;
  U32Queue incomingQueue;
  U32Queue outgoingQueue;
  volatile bool resetFlag = false;
  volatile bool needsClear = false;
  volatile bool isEnabled = false;

  void copyState() {
    if (incomingQueue.isReading())
      return;

    if (needsClear) {
      incomingQueue.clear();
      needsClear = false;
    }

    while (!newIncomingQueue.isEmpty() && !incomingQueue.isFull())
      incomingQueue.push(newIncomingQueue.pop());
  }

  void resetState() {
    needsClear = false;
    newIncomingQueue.clear();
    if (incomingQueue.isReading())
      needsClear = true;
    else
      incomingQueue.clear();
    outgoingQueue.syncClear();
    resetFlag = false;

    newIncomingQueue.overflow = false;
  }

  void setPendingData() {
    setData(outgoingQueue.isWriting() ? 0 : outgoingQueue.pop());
  }

  void setData(u32 data) {
    Link::_REG_JOY_TRANS_H = Link::msB32(data);
    Link::_REG_JOY_TRANS_L = Link::lsB32(data);
  }

  u32 getData() {
    return Link::buildU32(Link::_REG_JOY_RECV_H, Link::_REG_JOY_RECV_L);
  }

  void stop() {
    setInterruptsOff();
    setGeneralPurposeMode();
  }

  void start() {
    setJoybusMode();
    setInterruptsOn();
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

#endif  // LINK_CUBE_H
