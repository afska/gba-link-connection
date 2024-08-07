#ifndef LINK_SPI_H
#define LINK_SPI_H

// --------------------------------------------------------------------------
// An SPI handler for the Link Port (Normal Mode, either 32 or 8 bits).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkSPI* linkSPI = new LinkSPI();
// - 2) (Optional) Add the interrupt service routines: (*)
//       irq_init(NULL);
//       irq_add(II_SERIAL, LINK_SPI_ISR_SERIAL);
//       // (this is only required for `transferAsync`)
// - 3) Initialize the library with:
//       linkSPI->activate(LinkSPI::Mode::MASTER_256KBPS);
//       // (use LinkSPI::Mode::SLAVE on the other end)
// - 4) Exchange 32-bit data with the other end:
//       u32 data = linkSPI->transfer(0x12345678);
//       // (this blocks the console indefinitely)
// - 5) Exchange data with a cancellation callback:
//       u32 data = linkSPI->transfer(0x12345678, []() {
//         u16 keys = ~REG_KEYS & KEY_ANY;
//         return keys & KEY_START;
//       });
// - 6) Exchange data asynchronously:
//       linkSPI->transferAsync(0x12345678);
//       // ...
//       if (linkSPI->getAsyncState() == LinkSPI::AsyncState::READY) {
//         u32 data = linkSPI->getAsyncData();
//         // ...
//       }
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// considerations:
// - when using Normal Mode between two GBAs, use a GBC Link Cable!
// - only use the 2Mbps mode with custom hardware (very short wires)!
// - don't send 0xFFFFFFFF (or 0xFF in 8-bit mode), it's reserved for errors!
// --------------------------------------------------------------------------

#include "_link_common.hpp"

/**
 * @brief 8-bit mode (uncomment to enable)
 */
// #define LINK_SPI_8BIT_MODE

static volatile char LINK_SPI_VERSION[] = "LinkSPI/v7.0.0";

#ifdef LINK_SPI_8BIT_MODE
#define LINK_SPI_DATA_TYPE u8
#endif
#ifndef LINK_SPI_8BIT_MODE
#define LINK_SPI_DATA_TYPE u32
#endif

#ifdef LINK_SPI_8BIT_MODE
#define LINK_SPI_DATA_REG Link::_REG_SIODATA8
#endif
#ifndef LINK_SPI_8BIT_MODE
#define LINK_SPI_DATA_REG Link::_REG_SIODATA32
#endif

#ifdef LINK_SPI_8BIT_MODE
#define LINK_SPI_NO_DATA 0xff
#endif
#ifndef LINK_SPI_8BIT_MODE
#define LINK_SPI_NO_DATA 0xffffffff
#endif

/**
 * @brief An SPI handler for the Link Port (Normal Mode, either 32 or 8 bits).
 * 32-bit transfers by default. Set `LINK_SPI_8BIT_MODE` for 8-bit transfers.
 */
class LinkSPI {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int BIT_CLOCK = 0;
  static constexpr int BIT_CLOCK_SPEED = 1;
  static constexpr int BIT_SI = 2;
  static constexpr int BIT_SO = 3;
  static constexpr int BIT_START = 7;
  static constexpr int BIT_LENGTH = 12;
  static constexpr int BIT_IRQ = 14;
  static constexpr int BIT_GENERAL_PURPOSE_LOW = 14;
  static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

 public:
  enum Mode { SLAVE, MASTER_256KBPS, MASTER_2MBPS };
  enum AsyncState { IDLE, WAITING, READY };

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library in a specific `mode`.
   *
   * @param mode One of `LinkSPI::Mode::SLAVE`, `LinkSPI::Mode::MASTER_256KBPS`,
   * or `LinkSPI::Mode::MASTER_2MBPS`.
   */
  void activate(Mode mode) {
    this->mode = mode;
    this->waitMode = false;
    this->asyncState = IDLE;
    this->asyncData = 0;

    setNormalMode32Bit();
    disableTransfer();

    if (mode == SLAVE)
      setSlaveMode();
    else {
      setMasterMode();

      if (mode == MASTER_256KBPS)
        set256KbpsSpeed();
      else if (mode == MASTER_2MBPS)
        set2MbpsSpeed();
    }

    isEnabled = true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    setGeneralPurposeMode();

    mode = SLAVE;
    waitMode = false;
    asyncState = IDLE;
    asyncData = 0;
  }

  /**
   * @brief Exchanges `data` with the other end. Returns the received data.
   * @param data The value to be sent.
   */
  LINK_SPI_DATA_TYPE transfer(LINK_SPI_DATA_TYPE data) {
    return transfer(data, []() { return false; });
  }

  /**
   * @brief Exchanges `data` with the other end. Returns the received data.
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the response will be empty.
   */
  template <typename F>
  LINK_SPI_DATA_TYPE transfer(LINK_SPI_DATA_TYPE data,
                              F cancel,
                              bool _async = false,
                              bool _customAck = false) {
    if (asyncState != IDLE)
      return LINK_SPI_NO_DATA;

    setData(data);

    if (_async) {
      asyncState = WAITING;
      setInterruptsOn();
    } else {
      setInterruptsOff();
    }

    while (isMaster() && waitMode && !isSlaveReady())
      if (cancel()) {
        disableTransfer();
        setInterruptsOff();
        asyncState = IDLE;
        return LINK_SPI_NO_DATA;
      }

    enableTransfer();
    startTransfer();

    if (_async)
      return LINK_SPI_NO_DATA;

    while (!isReady())
      if (cancel()) {
        stopTransfer();
        disableTransfer();
        return LINK_SPI_NO_DATA;
      }

    if (!_customAck)
      disableTransfer();

    return getData();
  }

  /**
   * @brief Schedules a `data` transfer and returns. After this, call
   * `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the
   * async data, normal `transfer(...)`s won't do anything!
   * @param data The value to be sent.
   */
  void transferAsync(LINK_SPI_DATA_TYPE data) {
    transfer(data, []() { return false; }, true);
  }

  /**
   * @brief Schedules a `data` transfer and returns. After this, call
   * `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the
   * async data, normal `transfer(...)`s won't do anything!
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the response will be empty.
   */
  template <typename F>
  void transferAsync(LINK_SPI_DATA_TYPE data, F cancel) {
    transfer(data, cancel, true);
  }

  /**
   * @brief Returns the state of the last async transfer (one of
   * `LinkSPI::AsyncState::IDLE`, `LinkSPI::AsyncState::WAITING`, or
   * `LinkSPI::AsyncState::READY`).
   */
  [[nodiscard]] AsyncState getAsyncState() { return asyncState; }

  /**
   * @brief If the async state is `READY`, returns the remote data and switches
   * the state back to `IDLE`. If not, returns an empty response.
   */
  [[nodiscard]] LINK_SPI_DATA_TYPE getAsyncData() {
    if (asyncState != READY)
      return LINK_SPI_NO_DATA;

    LINK_SPI_DATA_TYPE data = asyncData;
    asyncState = IDLE;
    return data;
  }

  /**
   * @brief Returns the current `mode`.
   */
  [[nodiscard]] Mode getMode() { return mode; }

  /**
   * @brief Enables or disables `waitMode`: The GBA adds an extra feature over
   * SPI. When working as master, it can check whether the other terminal is
   * ready to receive (ready: `MISO=LOW`), and wait if it's not (not ready:
   * `MISO=HIGH`). That makes the connection more reliable, but it's not always
   * supported on other hardware units (e.g. the Wireless Adapter), so it must
   * be disabled in those cases.
   * \warning `waitMode` is disabled by default.
   * \warning `MISO` means `SO` on the slave side and `SI` on the master side.
   */
  void setWaitModeActive(bool isActive) { waitMode = isActive; }

  /**
   * @brief Returns whether `waitMode` (*) is active or not.
   * \warning See `setWaitModeActive(...)`.
   */
  [[nodiscard]] bool isWaitModeActive() { return waitMode; }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial(bool _customAck = false) {
    if (!isEnabled || asyncState != WAITING)
      return;

    if (!_customAck)
      disableTransfer();

    setInterruptsOff();
    asyncState = READY;
    asyncData = getData();
  }

  /**
   * @brief Sets SO output to HIGH.
   * \warning This is internal API!
   */
  void _setSOHigh() { setBitHigh(BIT_SO); }

  /**
   * @brief Sets SO output to LOW.
   * \warning This is internal API!
   */
  void _setSOLow() { setBitLow(BIT_SO); }

  /**
   * @brief Returns whether SI is HIGH or LOW.
   * \warning This is internal API!
   */
  [[nodiscard]] bool _isSIHigh() { return isBitHigh(BIT_SI); }

 private:
  Mode mode = Mode::SLAVE;
  bool waitMode = false;
  AsyncState asyncState = IDLE;
  LINK_SPI_DATA_TYPE asyncData = 0;
  volatile bool isEnabled = false;

  void setNormalMode32Bit() {
    Link::_REG_RCNT = Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_HIGH);
#ifdef LINK_SPI_8BIT_MODE
    Link::_REG_SIOCNT = 0;
#endif
#ifndef LINK_SPI_8BIT_MODE
    Link::_REG_SIOCNT = 1 << BIT_LENGTH;
#endif
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  void setData(LINK_SPI_DATA_TYPE data) { LINK_SPI_DATA_REG = data; }
  LINK_SPI_DATA_TYPE getData() { return LINK_SPI_DATA_REG; }

  void enableTransfer() { _setSOLow(); }
  void disableTransfer() { _setSOHigh(); }
  void startTransfer() { setBitHigh(BIT_START); }
  void stopTransfer() { setBitLow(BIT_START); }
  bool isReady() { return !isBitHigh(BIT_START); }
  bool isSlaveReady() { return !_isSIHigh(); }

  void setMasterMode() { setBitHigh(BIT_CLOCK); }
  void setSlaveMode() { setBitLow(BIT_CLOCK); }
  void set256KbpsSpeed() { setBitLow(BIT_CLOCK_SPEED); }
  void set2MbpsSpeed() { setBitHigh(BIT_CLOCK_SPEED); }
  void setInterruptsOn() { setBitHigh(BIT_IRQ); }
  void setInterruptsOff() { setBitLow(BIT_IRQ); }

  bool isMaster() { return mode != SLAVE; }
  bool isBitHigh(u8 bit) { return (Link::_REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { Link::_REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { Link::_REG_SIOCNT &= ~(1 << bit); }
};

extern LinkSPI* linkSPI;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_SPI_ISR_SERIAL() {
  linkSPI->_onSerial();
}

#endif  // LINK_SPI_H
