#ifndef LINK_CABLE_MULTIBOOT_H
#define LINK_CABLE_MULTIBOOT_H

// --------------------------------------------------------------------------
// A Multiboot tool to send small programs from one GBA to up to 3 slaves.
// --------------------------------------------------------------------------
// Usage:
// - 1) Include this header in your main.cpp file and add:
//       LinkCableMultiboot* linkCableMultiboot = new LinkCableMultiboot();
// - 2) Send the ROM:
//       LinkCableMultiboot::Result result = linkCableMultiboot->sendRom(
//         romBytes, // for current ROM, use: ((const u8*)MEM_EWRAM)
//         romLength, // in bytes, should be multiple of 0x10
//         []() {
//           u16 keys = ~REG_KEYS & KEY_ANY;
//           return keys & KEY_START;
//           // (when this returns true, the transfer will be canceled)
//         }
//       );
//       // `result` should be LinkCableMultiboot::Result::SUCCESS
// --------------------------------------------------------------------------
// considerations:
// - for better results, turn on the GBAs after calling the `sendRom` method!
// - stop DMA before sending the ROM! (you might need to stop your audio player)
// --------------------------------------------------------------------------

#include "LinkRawCable.hpp"

static volatile char LINK_CABLE_MULTIBOOT_VERSION[] =
    "LinkCableMultiboot/v7.0.0";

#define LINK_CABLE_MULTIBOOT_TRY(CALL)    \
  do {                                    \
    partialResult = CALL;                 \
  } while (partialResult == NEEDS_RETRY); \
  if (partialResult == ABORTED)           \
    return error(CANCELED);               \
  else if (partialResult == ERROR)        \
    return error(FAILURE_DURING_HANDSHAKE);

/**
 * @brief A Multiboot tool to send small programs from one GBA to up to 3
 * slaves.
 */
class LinkCableMultiboot {
 private:
  using u32 = unsigned int;
  using u16 = unsigned short;
  using u8 = unsigned char;

  static constexpr int MIN_ROM_SIZE = 0x100 + 0xc0;
  static constexpr int MAX_ROM_SIZE = 256 * 1024;
  static constexpr int WAIT_BEFORE_RETRY = (160 + 68) * 60;
  static constexpr int WAIT_BEFORE_TRANSFER = 50;
  static constexpr int DETECTION_TRIES = 16;
  static constexpr int PALETTE_DATA = 0x93;
  static constexpr int CLIENTS = 3;
  static constexpr int CLIENT_NO_DATA = 0xff;
  static constexpr int HANDSHAKE = 0x6200;
  static constexpr int HANDSHAKE_RESPONSE = 0x7200;
  static constexpr int CONFIRM_CLIENTS = 0x6100;
  static constexpr int SEND_PALETTE = 0x6300;
  static constexpr int HANDSHAKE_DATA = 0x11;
  static constexpr int CONFIRM_HANDSHAKE_DATA = 0x6400;
  static constexpr int ACK_RESPONSE = 0x73;
  static constexpr int HEADER_SIZE = 0xC0;
  static constexpr int SWI_MULTIPLAYER_MODE = 1;
  static constexpr auto MAX_BAUD_RATE = LinkRawCable::BaudRate::BAUD_RATE_3;
  static constexpr u8 CLIENT_IDS[] = {0b0010, 0b0100, 0b1000};

 public:
  enum Result {
    SUCCESS,
    INVALID_SIZE,
    CANCELED,
    FAILURE_DURING_HANDSHAKE,
    FAILURE_DURING_TRANSFER
  };

  /**
   * @brief Sends the `rom`. Once completed, the return value should be
   * `LinkCableMultiboot::Result::SUCCESS`.
   * @param rom A pointer to ROM data.
   * @param romSize Size of the ROM in bytes. It must be a number between `448`
   * and `262144`, and a multiple of `16`.
   * @param cancel A function that will be continuously invoked. If it returns
   * `true`, the transfer will be aborted.
   * \warning Blocks the system until completion or cancellation.
   */
  template <typename F>
  Result sendRom(const u8* rom, u32 romSize, F cancel) {
    if (romSize < MIN_ROM_SIZE)
      return INVALID_SIZE;
    if (romSize > MAX_ROM_SIZE)
      return INVALID_SIZE;
    if ((romSize % 0x10) != 0)
      return INVALID_SIZE;

    PartialResult partialResult;
    Link::_MultiBootParam multiBootParameters;
    multiBootParameters.client_data[0] = CLIENT_NO_DATA;
    multiBootParameters.client_data[1] = CLIENT_NO_DATA;
    multiBootParameters.client_data[2] = CLIENT_NO_DATA;
    multiBootParameters.palette_data = PALETTE_DATA;
    multiBootParameters.client_bit = 0;
    multiBootParameters.boot_srcp = (u8*)rom + HEADER_SIZE;
    multiBootParameters.boot_endp = (u8*)rom + romSize;

    LINK_CABLE_MULTIBOOT_TRY(detectClients(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(confirmClients(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(sendHeader(rom, cancel))
    LINK_CABLE_MULTIBOOT_TRY(confirmHeader(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(reconfirm(multiBootParameters, cancel))
    LINK_CABLE_MULTIBOOT_TRY(sendPalette(multiBootParameters, cancel))

    multiBootParameters.handshake_data =
        (HANDSHAKE_DATA + multiBootParameters.client_data[0] +
         multiBootParameters.client_data[1] +
         multiBootParameters.client_data[2]) %
        256;

    LINK_CABLE_MULTIBOOT_TRY(confirmHandshakeData(multiBootParameters, cancel))

    int result = Link::_MultiBoot(&multiBootParameters, SWI_MULTIPLAYER_MODE);

    linkRawCable->deactivate();

    return result == 1 ? FAILURE_DURING_TRANSFER : SUCCESS;
  }

  ~LinkCableMultiboot() { delete linkRawCable; }

 private:
  LinkRawCable* linkRawCable = new LinkRawCable();

  enum PartialResult { NEEDS_RETRY, FINISHED, ABORTED, ERROR };

  struct Responses {
    u16 d[CLIENTS];
  };

  template <typename F>
  PartialResult detectClients(Link::_MultiBootParam& multiBootParameters,
                              F cancel) {
    linkRawCable->activate(MAX_BAUD_RATE);

    for (u32 t = 0; t < DETECTION_TRIES; t++) {
      auto response = linkRawCable->transfer(HANDSHAKE, cancel);
      if (cancel())
        return ABORTED;

      for (u32 i = 0; i < CLIENTS; i++) {
        if ((response.data[1 + i] & 0xfff0) == HANDSHAKE_RESPONSE) {
          auto clientId = response.data[1 + i] & 0xf;

          switch (clientId) {
            case 0b0010:
            case 0b0100:
            case 0b1000: {
              multiBootParameters.client_bit |= clientId;
              break;
            }
            default:
              return NEEDS_RETRY;
          }
        }
      }
    }

    if (multiBootParameters.client_bit == 0) {
      linkRawCable->deactivate();
      wait(WAIT_BEFORE_RETRY);
      return NEEDS_RETRY;
    }

    return FINISHED;
  }

  template <typename F>
  PartialResult confirmClients(Link::_MultiBootParam& multiBootParameters,
                               F cancel) {
    return compare(multiBootParameters,
                   CONFIRM_CLIENTS | multiBootParameters.client_bit,
                   HANDSHAKE_RESPONSE, cancel);
  }

  template <typename F>
  PartialResult confirmHeader(Link::_MultiBootParam& multiBootParameters,
                              F cancel) {
    return compare(multiBootParameters, HANDSHAKE, 0, cancel);
  }

  template <typename F>
  PartialResult reconfirm(Link::_MultiBootParam& multiBootParameters,
                          F cancel) {
    return compare(multiBootParameters, HANDSHAKE, HANDSHAKE_RESPONSE, cancel);
  }

  template <typename F>
  PartialResult sendHeader(const u8* rom, F cancel) {
    u16* headerOut = (u16*)rom;

    for (int i = 0; i < HEADER_SIZE; i += 2) {
      linkRawCable->transfer(*(headerOut++), cancel);
      if (cancel())
        return ABORTED;
    }

    return FINISHED;
  }

  template <typename F>
  PartialResult sendPalette(Link::_MultiBootParam& multiBootParameters,
                            F cancel) {
    auto data = SEND_PALETTE | PALETTE_DATA;

    auto response = linkRawCable->transfer(data, cancel);
    if (cancel())
      return ABORTED;

    for (u32 i = 0; i < CLIENTS; i++) {
      if (response.data[1 + i] >> 8 == ACK_RESPONSE)
        multiBootParameters.client_data[i] = response.data[1 + i] & 0xff;
    }

    for (u32 i = 0; i < CLIENTS; i++) {
      u8 clientId = CLIENT_IDS[i];
      bool isClientConnected = multiBootParameters.client_bit & clientId;

      if (isClientConnected &&
          multiBootParameters.client_data[i] == CLIENT_NO_DATA)
        return NEEDS_RETRY;
    }

    return FINISHED;
  }

  template <typename F>
  PartialResult confirmHandshakeData(Link::_MultiBootParam& multiBootParameters,
                                     F cancel) {
    u16 data = CONFIRM_HANDSHAKE_DATA | multiBootParameters.handshake_data;
    auto response = linkRawCable->transfer(data, cancel);
    if (cancel())
      return ABORTED;

    return (response.data[1] >> 8) == ACK_RESPONSE ? FINISHED : ERROR;
  }

  template <typename F>
  PartialResult compare(Link::_MultiBootParam& multiBootParameters,
                        u16 data,
                        u16 expectedResponse,
                        F cancel) {
    auto response = linkRawCable->transfer(data, cancel);
    if (cancel())
      return ABORTED;

    for (u32 i = 0; i < CLIENTS; i++) {
      u8 clientId = CLIENT_IDS[i];
      u16 expectedResponseWithId = expectedResponse | clientId;
      bool isClientConnected = multiBootParameters.client_bit & clientId;

      if (isClientConnected && response.data[1 + i] != expectedResponseWithId)
        return ERROR;
    }

    return FINISHED;
  }

  Result error(Result error) {
    linkRawCable->deactivate();
    return error;
  }

  void wait(u32 verticalLines) {
    u32 count = 0;
    u32 vCount = Link::_REG_VCOUNT;

    while (count < verticalLines) {
      if (Link::_REG_VCOUNT != vCount) {
        count++;
        vCount = Link::_REG_VCOUNT;
      }
    };
  }
};

extern LinkCableMultiboot* linkCableMultiboot;

#endif  // LINK_CABLE_MULTIBOOT_H
