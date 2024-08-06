#ifndef LINK_RAW_CABLE_H
#define LINK_RAW_CABLE_H

// --------------------------------------------------------------------------
// A low level handler for the Link Port (Multi-Play Mode).
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkRawCable* linkRawCable = new LinkRawCable();
// - 2) (Optional) Add the interrupt service routines:
//       irq_init(NULL);
//       irq_add(II_SERIAL, LINK_RAW_CABLE_ISR_SERIAL);
//       // (this is only required for `transferAsync`)
// - 3) Initialize the library with:
//       linkRawCable->activate();
// - 4) Exchange 16-bit data with the connected consoles:
//       LinkRawCable::Response data = linkRawCable->transfer(0x1234);
//       // (this blocks the console indefinitely)
// - 5) Exchange data with a cancellation callback:
//       auto data = linkRawCable->transfer(0x1234, []() {
//         auto keys = ~REG_KEYS & KEY_ANY;
//         return keys & KEY_START;
//       });
// - 6) Exchange data asynchronously:
//       linkRawCable->transferAsync(0x1234);
//       // ...
//       if (linkRawCable->getAsyncState() == LinkRawCable::AsyncState::READY) {
//         auto data = linkRawCable->getAsyncData();
//         // ...
//       }
// --------------------------------------------------------------------------
// considerations:
// - don't send 0xFFFF, it's a reserved value that means <disconnected client>
// - only transfer(...) if isReady()
// --------------------------------------------------------------------------

#include "_link_common.h"

static volatile char LINK_RAW_CABLE_VERSION[] = "LinkRawCable/v6.4.0";

/**
 * @brief A low level handler for the Link Port (Multi-Play Mode).
 */
class LinkRawCable {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int MAX_PLAYERS = 4;
  static constexpr int DISCONNECTED = 0xffff;
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
  enum BaudRate {
    BAUD_RATE_0,  // 9600 bps
    BAUD_RATE_1,  // 38400 bps
    BAUD_RATE_2,  // 57600 bps
    BAUD_RATE_3   // 115200 bps
  };
  struct Response {
    u16 data[LinkRawCable::MAX_PLAYERS] = {DISCONNECTED, DISCONNECTED,
                                           DISCONNECTED, DISCONNECTED};
    int playerId = -1;  // (-1 = unknown)
  };
  enum AsyncState { IDLE, WAITING, READY };

 private:
  static constexpr Response EMPTY_RESPONSE = {
      {DISCONNECTED, DISCONNECTED, DISCONNECTED, DISCONNECTED},
      -1};

 public:
  /**
   * @brief Returns whether the library is active or not.
   */
  [[nodiscard]] bool isActive() { return isEnabled; }

  /**
   * @brief Activates the library in a specific `baudRate`.
   * @param baudRate One of the enum values from `LinkRawCable::BaudRate`.
   * Defaults to `LinkRawCable::BaudRate::BAUD_RATE_1` (38400 bps).
   */
  void activate(BaudRate baudRate = BaudRate::BAUD_RATE_1) {
    this->baudRate = baudRate;
    this->asyncState = IDLE;
    this->asyncData = LinkRawCable::EMPTY_RESPONSE;

    setMultiPlayMode();
    isEnabled = true;
  }

  /**
   * @brief Deactivates the library.
   */
  void deactivate() {
    isEnabled = false;
    setGeneralPurposeMode();

    baudRate = BaudRate::BAUD_RATE_1;
    asyncState = IDLE;
    asyncData = LinkRawCable::EMPTY_RESPONSE;
  }

  /**
   * @brief Exchanges `data` with the connected consoles. Returns the received
   * data from each player, including the assigned player id.
   * @param data The value to be sent.
   */
  Response transfer(u16 data) {
    return transfer(data, []() { return false; });
  }

  /**
   * @brief Exchanges `data` with the connected consoles. Returns the received
   * data from each player, including the assigned player id.
   * @param data The value to be sent.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted and the return value will be empty.
   */
  template <typename F>
  Response transfer(u16 data, F cancel, bool _async = false) {
    if (asyncState != IDLE)
      return LinkRawCable::EMPTY_RESPONSE;

    setData(data);

    if (_async) {
      asyncState = WAITING;
      setInterruptsOn();
    } else {
      setInterruptsOff();
    }

    startTransfer();

    if (_async)
      return LinkRawCable::EMPTY_RESPONSE;

    while (isSending())
      if (cancel()) {
        stopTransfer();
        return LinkRawCable::EMPTY_RESPONSE;
      }

    if (isReady() && !hasError())
      return getData();

    return LinkRawCable::EMPTY_RESPONSE;
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
   * @brief If the async state is `READY`, returns the remote data and switches
   * the state back to `IDLE`. If not, returns an empty response.
   */
  [[nodiscard]] Response getAsyncData() {
    if (asyncState != READY)
      return LinkRawCable::EMPTY_RESPONSE;

    Response data = asyncData;
    asyncState = IDLE;
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
  [[nodiscard]] bool isMaster() { return !isBitHigh(LinkRawCable::BIT_SLAVE); }

  /**
   * @brief Returns whether all connected consoles have entered the multiplayer
   * mode. Returns garbage when the cable is not properly connected.
   */
  [[nodiscard]] bool isReady() { return isBitHigh(LinkRawCable::BIT_READY); }

  /**
   * @brief Returns the state of the last async transfer (one of
   * `LinkRawCable::AsyncState::IDLE`, `LinkRawCable::AsyncState::WAITING`, or
   * `LinkRawCable::AsyncState::READY`).
   */
  [[nodiscard]] AsyncState getAsyncState() { return asyncState; }

  /**
   * @brief This method is called by the SERIAL interrupt handler. You shouldn't
   * call it manually!
   */
  void _onSerial() {
    if (!isEnabled || asyncState != WAITING)
      return;

    setInterruptsOff();
    asyncState = READY;
    asyncData = LinkRawCable::EMPTY_RESPONSE;
    if (isReady() && !hasError())
      asyncData = getData();
  }

 private:
  BaudRate baudRate = BaudRate::BAUD_RATE_1;
  AsyncState asyncState = IDLE;
  Response asyncData = LinkRawCable::EMPTY_RESPONSE;
  volatile bool isEnabled = false;

  void setMultiPlayMode() {
    Link::_REG_RCNT =
        Link::_REG_RCNT & ~(1 << LinkRawCable::BIT_GENERAL_PURPOSE_HIGH);
    Link::_REG_SIOCNT = (1 << LinkRawCable::BIT_MULTIPLAYER);
    Link::_REG_SIOCNT |= baudRate;
    Link::_REG_SIOMLT_SEND = 0;
  }

  void setGeneralPurposeMode() {
    Link::_REG_RCNT =
        (Link::_REG_RCNT & ~(1 << LinkRawCable::BIT_GENERAL_PURPOSE_LOW)) |
        (1 << LinkRawCable::BIT_GENERAL_PURPOSE_HIGH);
  }

  void setData(u16 data) { Link::_REG_SIOMLT_SEND = data; }
  Response getData() {
    Response response = LinkRawCable::EMPTY_RESPONSE;

    for (u32 i = 0; i < LinkRawCable::MAX_PLAYERS; i++)
      response.data[i] = Link::_REG_SIOMULTI[i];

    response.playerId =
        (Link::_REG_SIOCNT & (0b11 << LinkRawCable::BITS_PLAYER_ID)) >>
        LinkRawCable::BITS_PLAYER_ID;

    return response;
  }

  bool hasError() { return isBitHigh(LinkRawCable::BIT_ERROR); }
  bool isSending() { return isBitHigh(LinkRawCable::BIT_START); }

  void startTransfer() { setBitHigh(LinkRawCable::BIT_START); }
  void stopTransfer() { setBitLow(LinkRawCable::BIT_START); }

  void setInterruptsOn() { setBitHigh(LinkRawCable::BIT_IRQ); }
  void setInterruptsOff() { setBitLow(LinkRawCable::BIT_IRQ); }

  bool isBitHigh(u8 bit) { return (Link::_REG_SIOCNT >> bit) & 1; }
  void setBitHigh(u8 bit) { Link::_REG_SIOCNT |= 1 << bit; }
  void setBitLow(u8 bit) { Link::_REG_SIOCNT &= ~(1 << bit); }
};

extern LinkRawCable* linkRawCable;

inline void LINK_RAW_CABLE_ISR_SERIAL() {
  linkRawCable->_onSerial();
}

#endif  // LINK_RAW_CABLE_H
