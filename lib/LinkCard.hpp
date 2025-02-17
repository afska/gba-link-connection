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
// - 2) Send the DLC loader program:
//       if (device == LinkCard::ConnectedDevice::E_READER_USA)
//         LinkCard::SendResult result = linkCard->sendLoader(loader, []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//         }));
// - 3) Receive scanned cards:
//       if (device == LinkCard::ConnectedDevice::CARD_SCANNER) {
//         u8 card[1998];
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

LINK_VERSION_TAG LINK_CARD_VERSION = "vLinkCard/v8.0.0";

#define LINK_CARD_MAX_CARD_SIZE 2112

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
  static constexpr int MODE_SWITCH_WAIT = 228;
  static constexpr int PRE_TRANSFER_WAIT = 2;

 public:
  enum class SendResult {
    SUCCESS,
    UNALIGNED,
    INVALID_SIZE,
    CANCELED,
    WRONG_DEVICE,
    FAILURE_DURING_TRANSFER
  };
  enum class ConnectedDevice {
    E_READER_USA,
    E_READER_JAP,
    DLC_LOADER,
    WRONG_CONNECTION,
    UNKNOWN_DEVICE
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
    auto guard = Link::ScopeGuard([&]() { linkRawCable.deactivate(); });

    if (linkRawCable.transfer(0).playerId != 0)
      return ConnectedDevice::WRONG_CONNECTION;
    u16 remoteValues[3];
    for (u32 i = 0; i < 3; i++) {
      remoteValues[i] = transferMulti(0, []() { return false; });
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
   * Sends the loader card.
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

    {
      linkRawCable.activate();
      auto guard = Link::ScopeGuard([&]() { linkRawCable.deactivate(); });

      Link::wait(MODE_SWITCH_WAIT);
      if (cancel())
        return SendResult::CANCELED;

    retry:
      transferMulti(HANDSHAKE_SEND, cancel);
      if (transferMulti(HANDSHAKE_SEND, cancel) != deviceId)
        goto retry;
      if (transferMulti(deviceId, cancel) != deviceId)
        goto retry;
    }

    {
      linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);
      auto guard = Link::ScopeGuard([&]() { linkSPI.deactivate(); });

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

    {
      linkRawCable.activate();
      auto guard = Link::ScopeGuard([&]() { linkRawCable.deactivate(); });

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
   * Receives a 1998-byte `card` from the DLC Loader and returns `true` if it
   * worked correctly.
   * @param card A pointer to fill the card bytes.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  bool receiveCard(u8* card, F cancel) {
    LINK_READ_TAG(LINK_CARD_VERSION);
    // TODO: IMPLEMENT
    return true;
  }

 private:
  LinkRawCable linkRawCable;
  LinkSPI linkSPI;

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
};

extern LinkCard* linkCard;

#endif  // LINK_CARD_H
