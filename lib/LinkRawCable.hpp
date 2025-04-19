#ifndef LINK_RAW_CABLE_H
#define LINK_RAW_CABLE_H

// --------------------------------------------------------------------------
// A low level handler for the Link Port (Multi-Play Mode).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkRawCable* linkRawCable = new LinkRawCable();
// - 2) (Optional) Add the interrupt service routines: (*)
//       interrupt_init();
//       interrupt_add(INTR_SERIAL, LINK_RAW_CABLE_ISR_SERIAL);
//       // (this is only required for `transferAsync`)
// - 3) Initialize the library with:
//       linkRawCable->activate();
// - 4) Exchange 16-bit data with the connected consoles:
//       LinkRawCable::Response data = linkRawCable->transfer(0x1234);
//       // (this blocks the console indefinitely)
// - 5) Exchange data with a cancellation callback:
//       LinkRawCable::Response data = linkRawCable->transfer(0x1234, []() {
//         u16 keys = ~REG_KEYS & KEY_ANY;
//         return keys & KEY_START;
//       });
// - 6) Exchange data asynchronously:
//       linkRawCable->transferAsync(0x1234);
//       // ...
//       if (linkRawCable->getAsyncState() == LinkRawCable::AsyncState::READY) {
//         LinkRawCable::Response data = linkRawCable->getAsyncData();
//         // ...
//       }
// --------------------------------------------------------------------------
// (*) libtonc's interrupt handler sometimes ignores interrupts due to a bug.
//     That causes packet loss. You REALLY want to use libugba's instead.
//     (see examples)
// --------------------------------------------------------------------------
// considerations:
// - advanced usage only; if you're building a game, use `LinkCable`!
// - don't send 0xFFFF, it's a reserved value that means <disconnected client>!
// - only `transfer(...)` if `isReady()`!
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_RAW_CABLE_VERSION = "vLinkRawCable/v8.0.3";

#define LINK_RAW_CABLE_MAX_PLAYERS 4
#define LINK_RAW_CABLE_DISCONNECTED 0xFFFF

/**
 * @brief A low level handler for the Link Port (Multi-Play Mode).
 */
class LinkRawCable {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;

  static constexpr int BIT_SLAVE = 2;
  static constexpr int BIT_READY = 3;
  static constexpr int BITS_PLAYER_ID = 4;
  static constexpr int BIT_ERROR = 6;
  static constexpr int BIT_START = 7;
  static constexpr int BIT_MULTIPLAYER = 13;
  static constexpr int BIT_IRQ = 14;
  static constexpr int BIT_GENERAL_PURPOSE_LOW = 14;
  static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

 public:
  enum class BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  struct Response {
    u16 data[LINK_RAW_CABLE_MAX_PLAYERS];
    int playerId = -1;  // (-1 = unknown)
  };
  enum class AsyncState { IDLE, WAITING, READY };

 private:
  static constexpr Response EMPTY_RESPONSE = {
      {LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED,
       LINK_RAW_CABLE_DISCONNECTED, LINK_RAW_CABLE_DISCONNECTED},
      -1};

 public:
  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library in a specific `baudRate`.
   * @param baudRate One of the enum values from `LinkRawCable::BaudRate`.
   * Defaults to `LinkRawCable::BaudRate::BAUD_RATE_3` (115200 bps).
   */
  void activate(BaudRate baudRate = BaudRate::BAUD_RATE_3) {
    LINK_READ_TAG(LINK_RAW_CABLE_VERSION);

    this->baudRate = baudRate;
    this->asyncState = AsyncState::IDLE;
    this->asyncData = EMPTY_RESPONSE;

    setMultiPlayMode(baudRate);
    isEnabled = true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    setGeneralPurposeMode();

    baudRate = BaudRate::BAUD_RATE_1;
    asyncState = AsyncState::IDLE;
    asyncData = EMPTY_RESPONSE;
  }

  /**
   * @brief Exchanges `data` with the connected consoles. Returns the received
   * data from each player, including the assigned player ID.
   * @param data The value to be sent.
   * \warning Blocks the system until completion.
   */
  Response transfer(u16 data) {
    return transfer(data, []() { return false; });
  }

  /**
   * @brief Exchanges `data` with the connected consoles. Returns the received
   * data from each player, including the assigned player ID.
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the response will be empty.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  Response transfer(u16 data, F cancel, bool _async = false) {
    if (!isEnabled || asyncState != AsyncState::IDLE)
      return EMPTY_RESPONSE;

    setData(data);

    if (_async) {
      asyncState = AsyncState::WAITING;
      setInterruptsOn();
    } else {
      setInterruptsOff();
    }

    startTransfer();

    if (_async)
      return EMPTY_RESPONSE;

    while (isSending())
      if (cancel()) {
        stopTransfer();
        return EMPTY_RESPONSE;
      }

    if (isReady() && !hasError())
      return getData();

    return EMPTY_RESPONSE;
  }

  /**
   * @brief Schedules a `data` transfer and returns. After this, call
   * `getAsyncState()` and `getAsyncData()`. Note that until you retrieve the
   * async data, normal `transfer(...)`s won't do anything!
   * @param data The value to be sent.
   */
  void transferAsync(u16 data) {
    transfer(data, []() { return false; }, true);
  }

  /**
   * @brief Returns the state of the last async transfer
   * @return One of the enum values from `LinkRawCable::AsyncState`.
   */
  [[nodiscard]] AsyncState getAsyncState() { return asyncState; }

  /**
   * @brief If the async state is `READY`, returns the remote data and switches
   * the state back to `IDLE`. If not, returns an empty response.
   */
  [[nodiscard]] Response getAsyncData() {
    if (asyncState != AsyncState::READY)
      return EMPTY_RESPONSE;

    Response data = asyncData;
    asyncState = AsyncState::IDLE;
    return data;
  }

  /**
   * @brief Returns the current `baudRate`.
   */
  [[nodiscard]] BaudRate getBaudRate() { return baudRate; }

  /**
   * @brief Returns whether the console is connected as master or not. Returns
   * garbage when the cable is not properly connected.
   */
  [[nodiscard]] bool isMaster() { return isMasterNode(); }

  /**
   * @brief Returns whether all connected consoles have entered the multiplayer
   * mode. Returns garbage when the cable is not properly connected.
   */
  [[nodiscard]] bool isReady() { return allReady(); }

  /**
   * @brief This method is called by the SERIAL interrupt handler.
   * \warning This is internal API!
   */
  void _onSerial() {
    if (!isEnabled || asyncState != AsyncState::WAITING)
      return;

    setInterruptsOff();
    asyncState = AsyncState::READY;
    asyncData = EMPTY_RESPONSE;
    if (isReady() && !hasError())
      asyncData = getData();
  }

  // -------------
  // Low-level API
  // -------------
  static void setMultiPlayMode(BaudRate baudRate) {
    Link::_REG_RCNT = Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_HIGH);
    Link::_REG_SIOCNT = 1 << BIT_MULTIPLAYER;
    Link::_REG_SIOCNT |= (int)baudRate;
    Link::_REG_SIOMLT_SEND = 0;
  }
  static void setGeneralPurposeMode() {
    Link::_REG_SIOMLT_SEND = 0;
    Link::_REG_RCNT = (Link::_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                      (1 << BIT_GENERAL_PURPOSE_HIGH);
  }
  static void setData(u16 data) { Link::_REG_SIOMLT_SEND = data; }
  [[nodiscard]] static Response getData() {
    Response response = EMPTY_RESPONSE;

    for (u32 i = 0; i < LINK_RAW_CABLE_MAX_PLAYERS; i++)
      response.data[i] = Link::_REG_SIOMULTI[i];

    response.playerId =
        (Link::_REG_SIOCNT & (0b11 << BITS_PLAYER_ID)) >> BITS_PLAYER_ID;

    return response;
  }
  static void startTransfer() { setBitHigh(BIT_START); }
  static void stopTransfer() { setBitLow(BIT_START); }
  static void setInterruptsOn() { setBitHigh(BIT_IRQ); }
  static void setInterruptsOff() { setBitLow(BIT_IRQ); }
  [[nodiscard]] static bool isMasterNode() { return !isBitHigh(BIT_SLAVE); }
  [[nodiscard]] static bool allReady() { return isBitHigh(BIT_READY); }
  [[nodiscard]] static bool hasError() { return isBitHigh(BIT_ERROR); }
  [[nodiscard]] static bool isSending() { return isBitHigh(BIT_START); }
  // -------------

 private:
  BaudRate baudRate = BaudRate::BAUD_RATE_1;
  volatile AsyncState asyncState = AsyncState::IDLE;
  Response asyncData = EMPTY_RESPONSE;
  volatile bool isEnabled = false;

  static bool isBitHigh(u8 bit) { return (Link::_REG_SIOCNT >> bit) & 1; }
  static void setBitHigh(u8 bit) { Link::_REG_SIOCNT |= 1 << bit; }
  static void setBitLow(u8 bit) { Link::_REG_SIOCNT &= ~(1 << bit); }
};

extern LinkRawCable* linkRawCable;

/**
 * @brief SERIAL interrupt handler.
 */
inline void LINK_RAW_CABLE_ISR_SERIAL() {
  linkRawCable->_onSerial();
}

#endif  // LINK_RAW_CABLE_H
