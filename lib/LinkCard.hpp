#ifndef LINK_CARD_H
#define LINK_CARD_H

// --------------------------------------------------------------------------
// A library to receive DLCs from a second GBA using the e-Reader.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCard* linkCard = new LinkCard();
// - 2) Probe the connected device:
//       auto device = getConnectedDevice();
// - 3) Send the DLC loader program:
//       if (device == LinkCard::ConnectedDevice::E_READER_USA)
//         LinkCard::SendResult result = linkCard->sendLoader(loader, []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//         }));
// - 4) Receive scanned cards:
//       if (device == LinkCard::ConnectedDevice::DLC_LOADER) {
//         u8 card[LINK_CARD_SIZE];
//         bool received = linkCard->receiveCard(card, []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//         }));
//
//         if (received) {
//           // use `card` as DLC
//         }
//       }
// --------------------------------------------------------------------------

#ifndef LINK_DEVELOPMENT
#pragma GCC system_header
#endif

#include "LinkRawCable.hpp"
#include "LinkSPI.hpp"

#include "_link_common.hpp"

LINK_VERSION_TAG LINK_CARD_VERSION = "vLinkCard/v8.0.1";

#define LINK_CARD_SIZE 1998

#define LINK_CARD_USE_SHUTDOWN_PROTOCOL 0
// ^^^ just for testing with official loaders, `DLC Loader` doesn't use it!

/**
 * @brief A library to receive DLCs from a second GBA using the e-Reader.
 */
class LinkCard {
 private:
  using u32 = Link::u32;
  using u16 = Link::u16;
  using u8 = Link::u8;
  using vu32 = Link::vu32;

  static constexpr int MIN_LOADER_SIZE = 0x34 + 4;
  static constexpr int MAX_LOADER_SIZE = 0xEFFF + 1;
  static constexpr int HANDSHAKE_SEND = 0xFEFE;
  static constexpr int DEVICE_E_READER_USA = 0xCCC0;
  static constexpr int DEVICE_E_READER_JAP = 0xCCD0;
  static constexpr int DEVICE_LOADER = 0xFBFB;
  static constexpr int TRANSFER_SUCCESS = 0x1;
  static constexpr int HANDSHAKE_RECV_1 = 0xFBFB;
  static constexpr int HANDSHAKE_RECV_2 = 0x5841;
  static constexpr int HANDSHAKE_RECV_3 = 0x4534;
  static constexpr int GAME_ANIMATING = 0xF3F3;
  static constexpr int GAME_REQUEST = 0xECEC;
  static constexpr int GAME_READY = 0xEFEF;
  static constexpr int GAME_RECEIVE_READY = 0xFEFE;
  static constexpr int GAME_RECEIVE_OK = 0xF5F5;
  static constexpr int GAME_SIO_END = 0xF1F1;
  static constexpr int EREADER_ANIMATING = 0xF2F2;
  static constexpr int EREADER_READY = 0xF1F1;
  static constexpr int EREADER_SEND_READY = 0xF9F9;
  static constexpr int EREADER_SEND_START = 0xFDFD;
  static constexpr int EREADER_SEND_END = 0xFCFC;
  static constexpr int EREADER_SIO_END = 0xF3F3;
  static constexpr int EREADER_CANCEL = 0xF7F7;
  static constexpr int CMD_LINKCARD_RESET = 0;
  static constexpr int MODE_SWITCH_WAIT = 228;
  static constexpr int DEACTIVATION_WAIT = 50;
  static constexpr int PRE_TRANSFER_WAIT = 2 + 1;

 public:
  enum class ConnectedDevice {
    E_READER_USA,
    E_READER_JAP,
    DLC_LOADER,
    WRONG_CONNECTION,
    UNKNOWN_DEVICE
  };

  enum class SendResult {
    SUCCESS,
    UNALIGNED,
    INVALID_SIZE,
    CANCELED,
    WRONG_DEVICE,
    FAILURE_DURING_TRANSFER
  };

  enum class ReceiveResult {
    SUCCESS,
    CANCELED,
    WRONG_DEVICE,
    BAD_CHECKSUM,
    UNEXPECTED_FAILURE,
  };

  /**
   * Returns the connected device.
   * If it returns `E_READER_USA` or `E_READER_JAP`, you should send the loader
   * with `sendLoader(...)`.
   * If it returns `DLC_LOADER`, you should receive scanned cards with
   * `receiveCard(...)`.
   * \warning Blocks the system until completion.
   */
  ConnectedDevice getConnectedDevice() {
    return getConnectedDevice([]() { return false; });
  }

  /**
   * Returns the connected device.
   * If it returns `E_READER_USA` or `E_READER_JAP`, you should send the loader
   * with `sendLoader(...)`.
   * If it returns `DLC_LOADER`, you should receive scanned cards with
   * `receiveCard(...)`.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  ConnectedDevice getConnectedDevice(F cancel) {
    linkRawCable.activate();
    auto guard = Link::ScopeGuard([&]() { disableMulti(); });

    if (linkRawCable.transfer(CMD_LINKCARD_RESET, cancel).playerId != 0)
      return ConnectedDevice::WRONG_CONNECTION;
    u16 remoteValues[3];
    for (u32 i = 0; i < 3; i++) {
      remoteValues[i] = transferMulti(CMD_LINKCARD_RESET, cancel);
      if (i > 0 && remoteValues[i] != remoteValues[i - 1])
        return ConnectedDevice::UNKNOWN_DEVICE;
    }

    return remoteValues[0] == DEVICE_E_READER_USA
               ? ConnectedDevice::E_READER_USA
           : remoteValues[0] == DEVICE_E_READER_JAP
               ? ConnectedDevice::E_READER_JAP
           : remoteValues[0] == DEVICE_LOADER ? ConnectedDevice::DLC_LOADER
                                              : ConnectedDevice::UNKNOWN_DEVICE;
  }

  /**
   * Sends the loader card and returns a `SendResult`.
   * @param loader A pointer to a e-Reader program that sends
   * the scanned cards to the game. Must be 4-byte aligned.
   * @param loaderSize Size of the loader program in bytes. Must be a multiple
   * of 32.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  SendResult sendLoader(const u8* loader, u32 loaderSize, F cancel) {
    if ((u32)loader % 4 != 0)
      return SendResult::UNALIGNED;
    if (loaderSize < MIN_LOADER_SIZE || loaderSize > MAX_LOADER_SIZE ||
        (loaderSize % 0x20) != 0)
      return SendResult::INVALID_SIZE;

    auto device = getConnectedDevice(cancel);
    if (device != ConnectedDevice::E_READER_USA &&
        device != ConnectedDevice::E_READER_JAP)
      return SendResult::WRONG_DEVICE;

    auto deviceId = device == ConnectedDevice::E_READER_USA
                        ? DEVICE_E_READER_USA
                        : DEVICE_E_READER_JAP;

    // handshake
    {
      linkRawCable.activate();
      auto guard = Link::ScopeGuard([&]() { disableMulti(); });

      Link::wait(MODE_SWITCH_WAIT);
      if (cancel())
        return SendResult::CANCELED;

      while (true) {
        if (cancel())
          return SendResult::CANCELED;
        transferMulti(HANDSHAKE_SEND, cancel);
        if (transferMulti(HANDSHAKE_SEND, cancel) != deviceId)
          continue;
        if (transferMulti(deviceId, cancel) != deviceId)
          continue;
        break;
      }
    }

    // main transfer
    {
      linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);
      auto guard = Link::ScopeGuard([&]() { disableNormal(); });

      Link::wait(MODE_SWITCH_WAIT);
      if (cancel())
        return SendResult::CANCELED;

      transferNormal(loaderSize, cancel);

      u32 checksum = 0;
      u32* dataOut = (u32*)loader;
      for (u32 i = 0; i < loaderSize / 4; i++) {
        u32 data = dataOut[i];
        checksum += data;
        transferNormal(data, cancel);
      }

      transferNormal(0, cancel);
      transferNormal(checksum, cancel);
      transferNormal(checksum, cancel);
    }

    // confirmation
    {
      linkRawCable.activate();
      auto guard = Link::ScopeGuard([&]() { disableMulti(); });

      Link::wait(MODE_SWITCH_WAIT);
      if (cancel())
        return SendResult::CANCELED;

      if (transferMulti(deviceId, cancel) != deviceId ||
          transferMulti(deviceId, cancel) != TRANSFER_SUCCESS)
        return SendResult::FAILURE_DURING_TRANSFER;
    }

    return SendResult::SUCCESS;
  }

  /**
   * Receives a 1998-byte `card` from the DLC Loader and returns a
   * `ReceiveResult`.
   * @param card A pointer to fill the card bytes.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  ReceiveResult receiveCard(u8* card, F cancel) {
    LINK_READ_TAG(LINK_CARD_VERSION);

    auto device = getConnectedDevice(cancel);
    if (device != ConnectedDevice::DLC_LOADER)
      return ReceiveResult::WRONG_DEVICE;

    linkRawCable.activate();
    auto guard = Link::ScopeGuard([&]() { disableMulti(); });

    // handshake
    if (!transferMultiAndExpect(HANDSHAKE_RECV_1, HANDSHAKE_RECV_1, cancel))
      return ReceiveResult::CANCELED;
    if (!transferMultiAndExpect(HANDSHAKE_RECV_2, HANDSHAKE_RECV_2, cancel))
      return ReceiveResult::CANCELED;
    if (!transferMultiAndExpect(HANDSHAKE_RECV_3, HANDSHAKE_RECV_3, cancel))
      return ReceiveResult::CANCELED;

    // card request
    if (!transferMultiAndExpect(GAME_REQUEST, HANDSHAKE_RECV_3, cancel))
      return ReceiveResult::CANCELED;
    if (!transferMultiAndExpectOneOf(EREADER_ANIMATING, GAME_ANIMATING,
                                     EREADER_READY, cancel))
      return ReceiveResult::CANCELED;
    if (transferMultiAndExpectOneOf(EREADER_ANIMATING, EREADER_ANIMATING,
                                    EREADER_READY, cancel) == -1)
      return ReceiveResult::CANCELED;

    // wait for card
    while (true) {
      int received = 0;
      if ((received = transferMultiAndExpectOneOf(
               GAME_READY, EREADER_READY, EREADER_SEND_READY, cancel)) == -1)
        return ReceiveResult::CANCELED;
      if (received == EREADER_SEND_READY)
        break;
    }

    // start signal
    if (!transferMultiAndExpect(GAME_RECEIVE_READY, EREADER_SEND_READY, cancel))
      return ReceiveResult::CANCELED;
    if (!transferMultiAndExpect(GAME_RECEIVE_READY, EREADER_SEND_START, cancel))
      return ReceiveResult::CANCELED;

    // main transfer
    u32 checksum = 0;
    for (u32 i = 0; i < LINK_CARD_SIZE; i += 2) {
      if (cancel())
        return ReceiveResult::CANCELED;

      u16 block = transferMulti(GAME_RECEIVE_READY, cancel);
      card[i] = Link::lsB16(block);
      card[i + 1] = Link::msB16(block);
      checksum += block;
    }

    // checksum
    if (transferMulti(GAME_RECEIVE_READY, cancel) != Link::lsB32(checksum))
      return ReceiveResult::BAD_CHECKSUM;
    if (transferMulti(GAME_RECEIVE_READY, cancel) != Link::msB32(checksum))
      return ReceiveResult::BAD_CHECKSUM;

    // end
    if (transferMulti(GAME_RECEIVE_READY, cancel) != EREADER_SEND_END)
      return ReceiveResult::UNEXPECTED_FAILURE;

    // shutdown protocol (not needed, just for testing with official loaders)
#if LINK_CARD_USE_SHUTDOWN_PROTOCOL == 1
    if (!transferMultiAndExpect(GAME_RECEIVE_OK, EREADER_SIO_END, cancel))
      return ReceiveResult::CANCELED;
    transferMulti(GAME_SIO_END, cancel);
#endif

    return ReceiveResult::SUCCESS;
  }

 private:
  LinkRawCable linkRawCable;
  LinkSPI linkSPI;

  template <typename F>
  int transferMultiAndExpectOneOf(u16 value,
                                  u16 expected1,
                                  u16 expected2,
                                  F cancel) {
    u16 received;
    do {
      received = transferMulti(value, cancel);
      if (cancel() || received == EREADER_CANCEL)
        return -1;
    } while (received != expected1 && received != expected2);

    return received;
  }

  template <typename F>
  bool transferMultiAndExpect(u16 value, u16 expected, F cancel) {
    u16 received;
    do {
      received = transferMulti(value, cancel);
      if (cancel() || received == EREADER_CANCEL)
        return false;
    } while (received != expected);

    return true;
  }

  template <typename F>
  u16 transferMulti(u16 value, F cancel) {
    Link::wait(PRE_TRANSFER_WAIT);
    return linkRawCable.transfer(value, cancel).data[1];
  }

  template <typename F>
  void transferNormal(u32 value, F cancel) {
    Link::wait(PRE_TRANSFER_WAIT);
    linkSPI.transfer(value, cancel);
  }

  void disableMulti() {
    Link::wait(DEACTIVATION_WAIT);
    linkRawCable.deactivate();
  }

  void disableNormal() {
    Link::wait(DEACTIVATION_WAIT);
    linkSPI.deactivate();
  }
};

extern LinkCard* linkCard;

#endif  // LINK_CARD_H
