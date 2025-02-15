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
//       if (device == LinkCard::ConnectedDevice::E_READER_LOADER_NEEDED)
//         LinkCard::SendResult result = linkCard->sendLoader(loader);
// - 3) Receive scanned cards:
//       if (device == LinkCard::ConnectedDevice::CARD_SCANNER) {
//         u8 card[2076]
//         if (linkCard->receiveCard(card)) {
//           // use card as DLC
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
  static constexpr int DEVICE_E_READER = 0xCCC0;
  static constexpr int DEVICE_LOADER = 0xFBFB;

 public:
  enum class SendResult {
    SUCCESS,
    UNALIGNED,
    INVALID_SIZE,
    WRONG_DEVICE,
    FAILURE_DURING_TRANSFER
  };
  enum class ConnectedDevice {
    E_READER_LOADER_NEEDED,
    DLC_LOADER,
    WRONG_CONNECTION,
    UNKNOWN_DEVICE
  };

  /**
   * Returns the connected device.
   * If it returns `E_READER_LOADER_NEEDED`, you should send the loader with
   * `sendLoader(...)`.
   * If it returns `DLC_LOADER`, you should receive scanned cards with
   * `receiveCard(...)`.
   * If it returns `WRONG_CONNECTION`, the console is
   * connected with the wrong end (it has to use the 1P end, aka playerId=0).
   * If it returns `UNKNOWN_DEVICE`, the connected console uses another
   * protocol.
   */
  ConnectedDevice getConnectedDevice() {
    linkRawCable.activate();
    auto guard = Link::ScopeGuard([&]() { linkRawCable.deactivate(); });

    if (linkRawCable.transfer(0).playerId != 0)
      return ConnectedDevice::WRONG_CONNECTION;
    u16 remoteValues[3];
    for (u32 i = 0; i < 3; i++) {
      remoteValues[i] = linkRawCable.transfer(0).data[1];
      if (i > 0 && remoteValues[i] != remoteValues[i - 1])
        return ConnectedDevice::UNKNOWN_DEVICE;
    }

    return remoteValues[0] == DEVICE_E_READER
               ? ConnectedDevice::E_READER_LOADER_NEEDED
           : remoteValues[0] == DEVICE_LOADER ? ConnectedDevice::DLC_LOADER
                                              : ConnectedDevice::UNKNOWN_DEVICE;
  }

  /**
   * Sends the loader card.
   * @param loader A pointer to a e-Reader program that sends
   * the scanned cards to the game. Must be 4-byte aligned.
   * @param loaderSize Size of the loader program in bytes. Must be a multiple
   * of 32.
   */
  SendResult sendLoader(const u8* loader, u32 loaderSize) {
    if ((u32)loader % 4 != 0)
      return SendResult::UNALIGNED;
    if (loaderSize < MIN_LOADER_SIZE || loaderSize > MAX_LOADER_SIZE ||
        (loaderSize % 0x20) != 0)
      return SendResult::INVALID_SIZE;

    auto device = getConnectedDevice();
    if (device != ConnectedDevice::E_READER_LOADER_NEEDED)
      return SendResult::WRONG_DEVICE;

    linkRawCable.activate();
    u16 handshake = 0;
    while (handshake != DEVICE_E_READER) {
      handshake = linkRawCable.transfer(DEVICE_E_READER).data[1];
      // cancel?
    }
    linkRawCable.deactivate();

    linkSPI.activate(LinkSPI::Mode::MASTER_256KBPS);
    linkSPI.transfer(loaderSize);  // cancel?

    u32* dataOut = (u32*)loader;
    for (u32 i = 0; i < loaderSize / 4; i++) {
      linkSPI.transfer(dataOut[i]);
      // cancel?
    }

    while (true) {
    }

    return SendResult::SUCCESS;
  }

  bool receiveCard() {
    LINK_READ_TAG(LINK_CARD_VERSION);
    // TODO: IMPLEMENT
    return true;
  }

 private:
  LinkRawCable linkRawCable;
  LinkSPI linkSPI;
};

extern LinkCard* linkCard;

#endif  // LINK_CARD_H
