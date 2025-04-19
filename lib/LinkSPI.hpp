#ifndef LINK_SPI_H
#define LINK_SPI_H

// --------------------------------------------------------------------------
// An SPI handler for the Link Port (Normal Mode, either 32 or 8 bits).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkSPI* linkSPI = new LinkSPI();
// - 2) (Optional) Add the interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_SERIAL, LINK_SPI_ISR_SERIAL);
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
// - returns 0xFFFFFFFF (or 0xFF) on misuse or cancelled transfers!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_SPI_VERSION = "vLinkSPI/v8.0.3";

#define LINK_SPI_NO_DATA_32 0xFFFFFFFF
#define LINK_SPI_NO_DATA_8 0xFF
#define LINK_SPI_NO_DATA LINK_SPI_NO_DATA_32

/**
 * @brief An SPI handler for the Link Port (Normal Mode, either 32 or 8 bits).
 */
class LinkSPI {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu32 = Link::vu32;

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
  enum class Mode { SLAVE, MASTER_256KBPS, MASTER_2MBPS };
  enum class DataSize { SIZE_32BIT, SIZE_8BIT };
  enum class AsyncState { IDLE, WAITING, READY };

  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library in a specific `mode`.
   * @param mode One of the enum values from `LinkSPI::Mode`.
   * @param dataSize One of the enum values from `LinkSPI::DataSize`.
   */
  void activate(Mode mode, DataSize dataSize = DataSize::SIZE_32BIT) {
    LINK_READ_TAG(LINK_SPI_VERSION);

    this->mode = mode;
    this->dataSize = dataSize;
    this->waitMode = false;
    this->asyncState = AsyncState::IDLE;
    this->asyncData = 0;

    setNormalMode();
    disableTransfer();

    if (mode == Mode::SLAVE)
      setSlaveMode();
    else {
      setMasterMode();

      if (mode == Mode::MASTER_256KBPS)
        set256KbpsSpeed();
      else if (mode == Mode::MASTER_2MBPS)
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

    mode = Mode::SLAVE;
    waitMode = false;
    asyncState = AsyncState::IDLE;
    asyncData = 0;
  }

  /**
   * @brief Exchanges `data` with the other end. Returns the received data.
   * @param data The value to be sent.
   * \warning Blocks the system until completion.
   */
  u32 transfer(u32 data) {
    return transfer(data, []() { return false; });
  }

  /**
   * @brief Exchanges `data` with the other end. Returns the received data.
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the response will be empty.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  u32 transfer(u32 data,
               F cancel,
               bool _async = false,
               bool _customAck = false) {
    if ((!_customAck && !isEnabled) || asyncState != AsyncState::IDLE)
      return noData();

    setData(data);

    if (_async) {
      asyncState = AsyncState::WAITING;
      setInterruptsOn();
    } else {
      setInterruptsOff();
    }

    while (isMaster() && waitMode && !isSlaveReady())
      if (cancel()) {
        disableTransfer();
        setInterruptsOff();
        asyncState = AsyncState::IDLE;
        return noData();
      }

    enableTransfer();
    startTransfer();

    if (_async)
      return noData();

    while (!isReady())
      if (cancel()) {
        stopTransfer();
        disableTransfer();
        return noData();
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
   * \warning If `waitMode` (*) is active, blocks the system until completion.
   * See `setWaitModeActive(...)`.
   */
  void transferAsync(u32 data) {
    transfer(data, []() { return false; }, true);
  }

  /**
   * @brief Schedules a `data` transfer and returns. After this, call
   * `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the
   * async data, normal `transfer(...)`s won't do anything!
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the response will be empty.
   * \warning If `waitMode` (*) is active, blocks the system until completion or
   * cancellation. See `setWaitModeActive(...)`.
   */
  template <typename F>
  void transferAsync(u32 data, F cancel) {
    transfer(data, cancel, true);
  }

  /**
   * @brief Returns the state of the last async transfer.
   * @return One of the enum values from `LinkSPI::AsyncState`.
   */
  [[nodiscard]] AsyncState getAsyncState() { return asyncState; }

  /**
   * @brief If the async state is `READY`, returns the remote data and switches
   * the state back to `IDLE`. If not, returns an empty response.
   */
  [[nodiscard]] u32 getAsyncData() {
    if (asyncState != AsyncState::READY)
      return noData();

    u32 data = asyncData;
    asyncState = AsyncState::IDLE;
    return data;
  }

  /**
   * @brief Returns the current `mode`.
   */
  [[nodiscard]] Mode getMode() { return mode; }

  /**
   * @brief Returns the current `dataSize`.
   */
  [[nodiscard]] DataSize getDataSize() { return dataSize; }

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
  void setWaitModeActive(bool isActive) {
    if (!isEnabled)
      return;

    waitMode = isActive;
  }

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
    if (!isEnabled || asyncState != AsyncState::WAITING)
      return;

    if (!_customAck)
      disableTransfer();

    setInterruptsOff();
    asyncState = AsyncState::READY;
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
  DataSize dataSize = DataSize::SIZE_32BIT;
  bool waitMode = false;
  volatile AsyncState asyncState = AsyncState::IDLE;
  vu32 asyncData = 0;
  volatile bool isEnabled = false;

  void setNormalMode() {
    Link::_REG_RCNT = Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_HIGH);

    if (dataSize == DataSize::SIZE_32BIT)
      Link::_REG_SIOCNT = 1 << BIT_LENGTH;
    else
      Link::_REG_SIOCNT = 0;
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }

  void setData(u32 data) {
    if (dataSize == DataSize::SIZE_32BIT)
      Link::_REG_SIODATA32 = data;
    else
      Link::_REG_SIODATA8 = data & 0xFF;
  }

  u32 getData() {
    return dataSize == DataSize::SIZE_32BIT ? Link::_REG_SIODATA32
                                            : Link::_REG_SIODATA8 & 0xFF;
  }

  u32 noData() {
    return dataSize == DataSize::SIZE_32BIT ? LINK_SPI_NO_DATA_32
                                            : LINK_SPI_NO_DATA_8;
  }

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

  bool isMaster() { return mode != Mode::SLAVE; }
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
